#!/bin/python

from glob import glob
from pathlib import Path
from string import Template
import argparse
import os
import tempfile
import shutil
import sys
import git


def get_project_root():
    git_repo = git.Repo(search_parent_directories=True)
    return git_repo.git.rev_parse("--show-toplevel")


# Hacky way to include syscfg_gen functions until we modularize this nicely
sys.path.append(os.path.join(get_project_root(), "tools/scripts/config"))
from syscfg_gen import load_config, generate_defaults_str, find_alt_defaults

parser = argparse.ArgumentParser()

parser.add_argument("executable", help="Executable name")
parser.add_argument("binpath", help="Binary path")
parser.add_argument(
    "--template",
    default=os.path.join(
        get_project_root(), "tools/scripts/release/readme_template.txt"
    ),
    help="Path to readme template file",
)
parser.add_argument(
    "--syscfg",
    help="Path to app_config.yml to auto-generate default config string generators in readme",
)
parser.add_argument(
    "--eng_build", action="store_true", help="Used for packaging engineering builds"
)
parser.add_argument("--version", help="Custom version string to use (optional)")

args = parser.parse_args()


# Get git version string from version.c, which is what's included in the image
def get_source_version(version_filename):

    version = {
        "version_str": None,
        "git_sha": None,
        "is_dirty": False,
        "is_eng": False,
    }

    with open(version_filename, "r") as version_file:
        for line in version_file.readlines():
            if ".versionStr = " in line:
                version["version_str"] = line.strip().split('"')[1]
            if "VER_DIRTY_FLAG_OFFSET" in line:
                if line.strip().split(" << ")[0][-1] == "1" in line:
                    version["is_dirty"] = True
            if "VER_ENG_FLAG_OFFSET" in line:
                if line.strip().split(" << ")[0][-1] == "1" in line:
                    version["is_eng"] = True
            if ".gitSHA = " in line:
                git_sha = line.strip().split(" = ")[1][:-1]
                git_sha = git_sha.split("x")[
                    1
                ].upper()  # Remove the 0x prefix and uppercase
                version["git_sha"] = git_sha
                print("GIT_SHA:", git_sha)

    if version["version_str"] is None:
        raise IOError("Unable to find .versionStr")

    return version


git_repo = git.Repo(search_parent_directories=True)

# Check if there are any uncommitted changes right now
if git_repo.is_dirty():
    if not args.eng_build:
        print("FATAL: Repo is not clean!")
        sys.exit(-1)
    else:
        print("WARNING: Repo is not clean!!!")


# Get version string from source code
source_version = get_source_version(os.path.join(args.binpath, "version.c"))

if not args.eng_build and source_version["is_dirty"]:
    print("FATAL: Repo was not clean during build!")
    sys.exit(-1)

custom_version = args.version
if custom_version:
    if custom_version != source_version["version_str"]:
        print("ERROR: Source version and custom version do not match!")
        print(source_version["version_str"], args.version)
        sys.exit(-1)
else:
    # Since version wasn't passed, use git to figure it out
    git_version = git_repo.git.describe("--always", "--dirty", "--abbrev=8")
    git_last_tag = git_repo.git.describe("--tags", "--abbrev=0")

    if not args.eng_build and git_version != git_last_tag:
        # Don't allow non-eng release builds unless this specific commit is tagged
        print(f"ERROR: There have been commits since the last tag.")
        print(f"{git_version} != {git_last_tag}")
        sys.exit(-1)
    if git_version != source_version["version_str"]:
        print("ERROR: Source version and git version do not match!")
        print(source_version["version_str"], git_version)
        sys.exit(-1)

print(f"Source version: {source_version['version_str']}")

# Get all build files
glob_path = os.path.join(args.binpath, f"{args.executable}*")
files = sorted(glob(glob_path))

# Replace forward slash with two underscores in case the tag has any
source_version["version_str"] = source_version["version_str"].replace("/", "__")

# Add ENG- prefix for filenames
if source_version["is_eng"]:
    source_version["version_str"] = "ENG-" + source_version["version_str"]

# First name in sorted list is imagename.elf
base_name = os.path.splitext(os.path.basename(files[0]))[0]

# Add version to basename
new_base_name = f"{base_name}-{source_version['version_str']}"
new_exec_name = args.executable.replace(base_name, new_base_name)

default_configs = ""
if args.syscfg and os.path.exists(args.syscfg):
    config = load_config(args.syscfg)
    alt_defaults = find_alt_defaults(config)

    default_configs = "# Default Configurations\n"
    for default in alt_defaults:
        default_configs += f"## {default}\n"
        cfg_str = generate_defaults_str(config, default)
        default_configs += f"`cfg b64 {cfg_str}`\n"

# Variables to replace in readme_template.txt
template_subs = {
    "VERSION": source_version["version_str"],
    "ELF_DFU_BIN": f"{new_exec_name}.dfu.bin",
    "ELF_UNIFIED_BIN": f"{new_exec_name}.unified.bin",
    "DEFAULT_CONFIGS": default_configs,
}

# Use temporary directory to rename and zip files
with tempfile.TemporaryDirectory() as tmpdirname:
    # Copy renamed files to temporary directory
    for file in files:
        filename, extension = os.path.splitext(os.path.basename(file))
        new_name = filename.replace(base_name, new_base_name) + extension
        shutil.copyfile(file, os.path.join(tmpdirname, new_name))

    # Auto-generate readme file with install/update instructions
    # To change template, look at readme_template.txt
    with open(args.template, "r") as template_file, open(
        os.path.join(tmpdirname, "README.txt"), "w"
    ) as readme_file:
        src = Template(template_file.read())
        readme_str = src.substitute(template_subs)
        readme_file.write(readme_str)

        # Print README.txt to console so we can easily copy-paste for github release notes
        print("Contents of README.txt:\n")
        print(readme_str)

    # Create zip file with release files
    zipfile = shutil.make_archive(new_base_name, "zip", tmpdirname)

    # Create release directory
    release_dir = os.path.join(args.binpath, "release")
    Path(release_dir).mkdir(exist_ok=True)

    # Move zip file to release directory
    new_zipfile = os.path.join(release_dir, os.path.basename(zipfile))
    shutil.move(zipfile, new_zipfile)

    print(f"Release archived in {new_zipfile}")
