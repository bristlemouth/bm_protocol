"""
Script to build required images (provided in yml config) and package
them in .zip files for release.
"""

import argparse
import git
import glob
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import yaml
from pprint import pprint


def get_project_root():
    git_repo = git.Repo(search_parent_directories=True)
    return git_repo.git.rev_parse("--show-toplevel")


# Try to securely delete a file.
# Opens file and overwrites it with random data several times
# before actually deleting it.
def secure_delete(filename, passes=5):
    if not os.path.exists(filename):
        return

    # Write a bunch of garbage to file before deleting
    with open(filename, "w+b") as file:
        length = file.tell()
        for _ in range(passes):
            file.seek(0)  # Go to start of file
            file.write(os.urandom(length))  # Write a bunch of garbage
            file.flush()  # Make sure new data is written to file

    os.remove(filename)


class AutoBuilder:
    def __init__(self, config_path, build_dir, verbose=False):
        self._project_root = get_project_root()
        print("Loading configs from", config_path)
        self._configs = self._load_configs(config_path)
        self.build_dir = build_dir
        self.verbose = verbose
        self.out_dir = None
        self.is_prod = False
        self.sign = False
        self.encrypt = False
        self.version = None
        self.ed25519_priv_key = None
        self.ed25519_priv_key_file = None
        self.x25519_priv_key = None
        self.x25519_priv_key_file = None

    # Get all the configs from the .yml file
    def _load_configs(self, path):
        if not os.path.exists(path):
            raise FileNotFoundError(f"{path} not found!")

        configs = {}
        with open(path, "r") as config:
            # TODO - validate config file
            yml_configs = yaml.safe_load(config)
            for config in yml_configs:
                configs[config["name"]] = config

        return configs

    # Run the cmake command to set up build environment for this config
    def setup_cmake_dir(self, config):
        project_root = get_project_root()

        cmd = [
            "cmake",
            f"{project_root}",
            f"-DCMAKE_TOOLCHAIN_FILE={project_root}/cmake/arm-none-eabi-gcc.cmake",
            f'-DBSP={config["args"]["bsp"]}',
            f'-DAPP={config["args"]["app"]}',
            f'-DCMAKE_BUILD_TYPE={config["args"]["build_type"]}',
        ]

        if config["name"] != "bootloader":
            cmd += ["-DUSE_BOOTLOADER=1"]

        if self.sign:
            cmd += ["-DSIGN_IMAGES=1"]
            if self.ed25519_priv_key_file:
                cmd += [f"-DED25519_KEY_FILE={self.ed25519_priv_key_file}"]

        if self.encrypt:
            cmd += ["-DENCRYPT_IMAGES=1"]
            if self.x25519_priv_key_file:
                cmd += [f"-DX25519_KEY_FILE={self.x25519_priv_key_file}"]

        if self.is_prod:
            cmd += ["-DRELEASE=1"]

        if self.version and len(self.version):
            # TODO - check for valid version string
            cmd += [f"-DVERSION={self.version}"]

        # If we built a bootloader, let's include it in the main app
        # TODO - don't create unified image when encryption is enabled,
        # since private key will be present on it (and unecrypted)
        if (
            config["name"] != "bootloader"
            and "bootloader" in self._configs
            and "elf_fullpath" in self._configs["bootloader"]
        ):
            cmd += [f'-DBOOTLOADER_PATH={self._configs["bootloader"]["elf_fullpath"]}']

        result = self.run_command(cmd)

    # Compile firmware image!
    def build_fw(self, config, jobs=8):
        cmd = ["make", "-j", f"{jobs}"]

        if self.verbose:
            cmd += ["VERBOSE=1"]
        result = self.run_command(cmd)

        # Grab .elf filename and size stats
        for line in result.stdout.split("\n"):
            match = re.match(
                r"^\s+(?P<text>[0-9]+)\s+(?P<data>[0-9]+)\s+(?P<bss>[0-9]+)\s+(?P<dec>[0-9]+)\s+(?P<hex>[0-9A-Za-z]+)\s+(?P<elf>[A-Za-z0-9\._-]+\.elf)",
                line,
            )
            if match:
                # add .elf file path to
                config["elf"] = match.group("elf")
                config["stats"] = {
                    "text": int(match.group("text")),
                    "data": int(match.group("data")),
                    "bss": int(match.group("bss")),
                }
                config["binpath"] = os.path.join(config["path"], "src")
                config["elf_fullpath"] = os.path.join(config["binpath"], config["elf"])

        # Make sure we were able to read out the .elf file
        if "elf" not in config:
            raise ValueError("Unable to read .elf filename from output")

    # Upload firmware symbols to memfault
    def memfault_upload(self, config):
        memfault_script = os.path.join(
            get_project_root(), "tools/scripts/memfault/memfault_elf_uploader.py"
        )
        cmd = [
            "python3",
            memfault_script,
            "--software_type",
            config["args"]["app"],
            config["elf_fullpath"],
        ]
        if self.verbose:
            cmd += ["--verbose"]
        result = self.run_command(cmd)

    # Create .zip file with appropriately named firmware images
    # TODO - move this to this file instead of calling release.py script
    def package_release(self, config):
        release_script = os.path.join(
            get_project_root(), "tools/scripts/release/release.py"
        )

        cmd = ["python3", release_script, config["elf"], config["binpath"]]

        if not self.is_prod:
            cmd += ["--eng_build"]

        if self.version and len(self.version):
            cmd += [f"--version={self.version}"]

        # Don't do syscfg/readme stuff for bootloader
        if config["args"]["app"] != "bootloader":
            app_cfg = os.path.join(
                get_project_root(), "src/apps/", config["args"]["app"], "app_config.yml"
            )
            cmd += ["--syscfg", app_cfg]

        result = self.run_command(cmd)

        # Grab .elf filename and size stats
        for line in result.stdout.split("\n"):
            match = re.match(
                r"^Release archived in (?P<zip>[A-Za-z0-9\/\-_\.\s]+\.zip)",
                line,
            )
            if match:
                # add .elf file path to
                config["zip"] = match.group("zip")

        # Make sure we were able to read out the .elf file
        if "zip" not in config:
            raise ValueError("Unable to read .zip archive filename from output")

    def run_command(self, command):
        if self.verbose:
            print(" ".join(command))

        result = subprocess.run(command, capture_output=True, text=True)

        if result.returncode != 0:
            print("\nSTDOUT:\n", result.stdout)
            print("\nSTDERR:\n", result.stderr)
            print("return code:", result.returncode)

            raise RuntimeError("Failed command ", " ".join(command))

        if self.verbose:
            print(result.stdout)

        return result

    def build(self, config):
        returncode = 0

        # Tests are run in temporary directory inside repository
        # This is so scripts that use/reference git will work (like version.c)

        previous_dir = os.getcwd()
        os.chdir(config["path"])

        try:

            # Run cmake setup commands
            self.setup_cmake_dir(config)

            # Compile firmware
            self.build_fw(config)

            # If this isn't a bootloader image, upload to memfault
            if config["args"]["app"] != "bootloader":
                self.memfault_upload(config)

            self.package_release(config)

            if self.out_dir and os.path.exists(config["zip"]):
                src = config["zip"]
                dest = os.path.join(self.out_dir, os.path.basename(config["zip"]))
                print(f"Copying {src} to {dest}")
                shutil.copy(src, dest)

        finally:
            # Always go back to where we started before the temp dir gets deleted
            os.chdir(previous_dir)

        return returncode

    def _setup_keys(self, tmpdirname):
        if self.sign and self.ed25519_priv_key:
            print("Creating temporary signing key file")
            self.ed25519_priv_key_file = os.path.join(tmpdirname, "ed25519_priv_key")
            with open(self.ed25519_priv_key_file, "w") as keyfile:
                keyfile.write(self.ed25519_priv_key)

        if self.encrypt and self.x25519_priv_key:
            print("Creating temporary encryption key file")
            self.x25519_priv_key_file = os.path.join(tmpdirname, "x25519_priv_key")
            with open(x25519_priv_key_file, "w") as keyfile:
                keyfile.write(self.x25519_priv_key)

        return

    # Remove keyfiles from system as best we can
    def _cleanup_keys(self, tmpdirname):
        if self.ed25519_priv_key_file:
            print("Removing temporary signing key file")
            secure_delete(self.ed25519_priv_key_file)

        if self.x25519_priv_key_file:
            print("Removing temporary encryption key file")
            secure_delete(self.x25519_priv_key_file)

        return

    def build_all(self):
        with tempfile.TemporaryDirectory(
            dir=args.build_dir, prefix="tmpbuild_"
        ) as tmpdirname:
            # Run through each test and see if it fails or not
            returncode = 0

            # Create temporary files with encryption/signing keys from
            # environment variables. (since we can't pass them as
            # arguments to imgtool.py to generate the C code)
            self._setup_keys(tmpdirname)

            for name, config in self._configs.items():
                print(f"Building: {name}")
                config["path"] = os.path.join(tmpdirname, get_name_hash(name, 16))
                os.mkdir(config["path"])

                # Cache sign config value
                old_sign = self.sign

                # Don't use signature support for non-prod bootloaders so they'll
                # accept any image
                if self.sign and (name == "bootloader") and not self.is_prod:
                    print("Disabling signature support for bootloader")
                    self.sign = False

                returncode = self.build(config)

                # Restore old sign config
                self.sign = old_sign

                if returncode != 0:
                    print("FAIL")
                    break

            # Delete temporary encryption/signing keys from disk
            self._cleanup_keys(tmpdirname)

            if returncode:
                sys.exit(returncode)


def get_name_hash(name, numchars=None):
    sha = hashlib.sha256()
    sha.update(name.encode())

    if numchars:
        return sha.hexdigest()[:numchars]
    else:
        return sha.hexdigest()


parser = argparse.ArgumentParser()
parser.add_argument("--verbose", action="store_true", help="Print more stuff")
parser.add_argument("--build_dir", default=get_project_root())
parser.add_argument(
    "--out_dir", help="Optional output directory for build artifacts (firmware images)"
)
parser.add_argument(
    "--prod", action="store_true", help="This is a production (non-eng) build"
)
parser.add_argument("--sign", action="store_true", help="Use signed images")
parser.add_argument("--encrypt", action="store_true", help="Use encrypted images")
parser.add_argument("--version", help="Version to use for release")
parser.add_argument(
    "config",
    help="Configuration (yml) file with build configuration.",
)
args = parser.parse_args()

builder = AutoBuilder(args.config, build_dir=args.build_dir, verbose=args.verbose)
if args.out_dir:
    # Make sure there's an artifacts dir to put things in
    if not os.path.exists(args.out_dir):
        print(f"Creating {args.out_dir}")
        pathlib.Path(args.out_dir).mkdir(parents=True, exist_ok=True)

    builder.out_dir = args.out_dir

builder.is_prod = args.prod
builder.sign = args.sign
builder.encrypt = args.encrypt
builder.version = args.version

# Get signing/encryption keys from env variables(optional)
# If these are not set and --sign or --encrypt are used, the development
# keys will be used
builder.ed25519_priv_key = os.getenv("ED25519_PRIV_KEY")
builder.x25519_priv_key = os.getenv("X25519_PRIV_KEY")

builder.build_all()
