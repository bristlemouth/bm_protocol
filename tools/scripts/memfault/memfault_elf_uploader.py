#!/usr/bin/env python3

import os
import sys
import subprocess
import glob
import git
import argparse

from dotenv import load_dotenv, find_dotenv

RELEASE_BRANCH = "main"


def get_git_branch():
    git_repo = git.Repo(search_parent_directories=True)
    return git_repo.git.branch("--show-current")


def get_project_root():
    git_repo = git.Repo(search_parent_directories=True)
    return git_repo.git.rev_parse("--show-toplevel")


"""
Example readelf -n output:

Displaying notes found in: .note.gnu.build-id
  Owner                Data size        Description
  GNU                  0x00000014       NT_GNU_BUILD_ID (unique build ID bitstring)
    Build ID: cebe50e6d5aa603cc53bb3d77f5177a287f42991
"""


def get_build_id(elf_file):
    notes = subprocess.getoutput(f"arm-none-eabi-readelf -n {elf_file}")

    # Return build id from the last line of the output
    # TODO - actually search for "Build ID: xxxxxxxx" since this will break
    # if we ever add another note after the build id
    build_id = notes.split("\n")[-1].split(":")[-1].strip()

    print(f'Using build id: "{build_id}"')

    return build_id


def get_git_version():
    return subprocess.getoutput("git describe --always --abbrev=8 --dirty")


if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("--software_type", default="unknown", help="Software type")
    parser.add_argument("--project", default="xmoonjelly", help="Memfault project name")
    parser.add_argument("--org", default="sofar-ocean", help="Memfault org")
    parser.add_argument("--version", help="Version string")
    parser.add_argument("--username", help="Memfault username/email")
    parser.add_argument("--password", help="Memfault password/api key")
    parser.add_argument("--org-token", help="Memfault org token")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("elf_file", help="Elf file to upload")
    args = parser.parse_args()

    load_dotenv(find_dotenv())

    elf_file = args.elf_file

    if args.version is None:
        version = get_git_version()
        if get_git_branch() != RELEASE_BRANCH:
            version = "ENG-" + version + "+" + get_build_id(elf_file)[:8]
    else:
        version = args.version

    # Put together memfault command
    memfault_cmd_args = [
        "memfault",
        "--org",
        args.org,
        "--project",
        args.project,
    ]

    # token or email/password need to precede the `upload-symbols` command
    if os.getenv("MEMFAULT_ORG_TOKEN"):
        memfault_cmd_args += [
            "--org-token",
            os.getenv("MEMFAULT_ORG_TOKEN")
            if args.org_token is None
            else args.org_token,
        ]
    elif os.getenv("MEMFAULT_USERNAME") and os.getenv("MEMFAULT_PASSWORD"):
        memfault_cmd_args += [
            "--email",
            os.getenv("MEMFAULT_USERNAME") if args.username is None else args.username,
            "--password",
            os.getenv("MEMFAULT_PASSWORD") if args.password is None else args.password,
        ]
    else:
        raise EnvironmentError("Memfault org token or username/password required!")

    memfault_cmd_args += [
        "upload-symbols",
        elf_file,
        "--software-type",
        args.software_type,
        "--software-version",
        version,
    ]

    if args.verbose:
        print(" ".join(memfault_cmd_args))

    subprocess.run(memfault_cmd_args)
