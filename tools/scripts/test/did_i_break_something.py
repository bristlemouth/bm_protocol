"""
Simple script to see if everything is building properly.
Ideally, this script will be run before pushing changes to github.
Eventually, a much nicer version of this will be used for CI

Usage: python3 did_i_break_something.py
"""

import subprocess
import tempfile
import argparse
import sys
import os
import git
import yaml
import glob


def get_project_root():
    git_repo = git.Repo(search_parent_directories=True)
    return git_repo.git.rev_parse("--show-toplevel")


# Get commands to build firmware project using test arguments
def build_fw_commands(test, verbose=False, jobs=4):
    project_root = get_project_root()
    app = test["args"]["app"]
    bsp = test["args"]["bsp"]
    build_type = test["args"]["build_type"]

    build_cmd = f"cmake {project_root} -DCMAKE_TOOLCHAIN_FILE={project_root}/cmake/arm-none-eabi-gcc.cmake -DBSP={bsp} -DAPP={app} -DCMAKE_BUILD_TYPE={build_type}"

    if "use_bootloader" in test["args"] and test["args"]["use_bootloader"]:
        build_cmd += " -DUSE_BOOTLOADER=1"

    if "defines" in test["args"]:
        build_cmd += " " + test["args"]["defines"]

    make_cmd = f"make -j {jobs} CC=gcc"
    if verbose:
        make_cmd += " VERBOSE=1"

    commands = [build_cmd.split(" "), make_cmd.split(" ")]

    return commands


# Get commands to build and run unit tests
def unit_test_commands(test, verbose=False, jobs=4):
    project_root = get_project_root()

    cmake_cmd = f"cmake {project_root}"
    ctest_cmd = "ctest -V"

    commands = [
        cmake_cmd.split(" "),
        f"make -j {jobs}".split(" "),
        ctest_cmd.split(" "),
    ]

    return commands


# Map configuration types to functions to get commands to run
test_handlers = {"build_fw": build_fw_commands, "unit_tests": unit_test_commands}


def run_tests_in_dir(dir, test, verbose=False):
    returncode = 0
    previous_dir = os.getcwd()
    os.chdir(dir)

    if test["type"] not in test_handlers:
        raise NotImplementedError("Unsupported test type!")

    # Get all commands for specific test
    commands = test_handlers[test["type"]](test, verbose)

    for command in commands:
        if verbose:
            print("Running ", " ".join(command))
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            print("\nSTDOUT:\n", result.stdout)
            print("\nSTDERR:\n", result.stderr)
            print("return code:", result.returncode)
            returncode = result.returncode
            break

    # Go back to where we started before the temp dir gets deleted
    os.chdir(previous_dir)

    return returncode


def run_test(test, verbose=False, store=False):
    returncode = 0

    # Tests are run in temporary directory inside repository
    # This is so scripts that use/reference git will work (like version.c)
    if store is False:
        with tempfile.TemporaryDirectory(
            dir=get_project_root(), prefix="tmpbuild_"
        ) as tmpdirname:
            returncode = run_tests_in_dir(tmpdirname, test, verbose)
    else:
        dir = get_project_root() + "/build/" + test["args"]["app"]
        if "sub" in test["args"]:
            dir += "/" + test["args"]["sub"]
        os.makedirs(dir, exist_ok=True)
        returncode = run_tests_in_dir(dir, test, verbose)

    return returncode


parser = argparse.ArgumentParser()
parser.add_argument("--verbose", action="store_true", help="Print more stuff")
parser.add_argument(
    "configs",
    nargs="+",
    help="Configuration (yaml) files with build/test configuration. Can also be directory with config files",
)
parser.add_argument(
    "--store",
    action="store_true",
    help="Store the tests in the build directory at the top of this repository",
)
args = parser.parse_args()


# TODO - gather build sizes, etc...

config_paths = []
for config in args.configs:
    if os.path.isdir(config):
        # If it's a directory, load all the yaml files
        config_paths += sorted(glob.glob(os.path.join(config, "*.yaml")))
    elif os.path.exists(config):
        # If it's a file, make sure it exists
        config_paths.append(config)
    else:
        raise FileNotFoundError(f"{config} not found!")

tests = []
for path in config_paths:
    with open(path, "r") as config:
        # TODO - validate config file
        tests += yaml.safe_load(config)

# Run through each test and see if it fails or not
for test in tests:
    print(f'Building: {test["name"]}')
    returncode = run_test(test, args.verbose, args.store)
    if returncode == 0:
        print("PASS")
    else:
        print("FAIL")
        # TODO - maybe run all the tests before exiting?
        sys.exit(returncode)
