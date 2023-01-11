#!/usr/bin/env python3

import argparse
import os
import subprocess
import git
import time
import tempfile
from dotenv import load_dotenv, find_dotenv
import signal


def get_project_root():
    git_repo = git.Repo(search_parent_directories=True)
    return os.path.realpath(git_repo.git.rev_parse("--show-toplevel"))


# Don't close python script with Ctrl+C since GDB uses it
def sig_handler(signum, frame):
    pass


# Register alternate SIGINT handler above to ignore Ctrl+C
signal.signal(signal.SIGINT, sig_handler)

debug_scripts_path = "{}/tools/scripts/debug".format(get_project_root())


#
# Create gdb script with memfault coredump information (gathered from .env)
# Since it needs to have user info to run
#
def create_coredump_gdb_script():
    memfault_gdb_path = os.path.join(
        get_project_root(),
        "src/third_party/memfault-firmware-sdk/scripts/memfault_gdb.py",
    )

    coredump_script = """
define coredump
  set $ram_start = (uint32_t)&_ram_start
  set $coredump_size =  (uint32_t)&__MemfaultCoreStorageEnd - (uint32_t)&__MemfaultCoreStorageStart
  source {script_path}
  memfault login {username} {password} -o sofar-ocean -p {project}
  memfault coredump --region $ram_start $coredump_size
end
""".format(
        script_path=memfault_gdb_path,
        username=os.getenv("MEMFAULT_USERNAME"),
        password=os.getenv("MEMFAULT_PASSWORD"),
        project=os.getenv("MEMFAULT_PROJECT"),
    )

    fd, path = tempfile.mkstemp(suffix=".gdb_cmds")
    os.write(fd, coredump_script.encode())
    os.close(fd)

    return path


def run_openocd(args):
    # OpenOCD config overrides
    part_cfg = "st_nucleo_u5.cfg"
    if args.rtos:
        part_cfg = "st_nucleo_u5_rtos.cfg"

    openocd_cmd = [
        "openocd",
        "-f",
        f"{debug_scripts_path}/stlink-mod.cfg",
        "-f",
        f"{debug_scripts_path}/{part_cfg}",
        "-f",
        f"{debug_scripts_path}/utils.cfg",
        "-c",
        "init",
    ]

    # https://stackoverflow.com/questions/5045771/python-how-to-prevent-subprocesses-from-receiving-ctrl-c-control-c-sigint
    return subprocess.Popen(
        openocd_cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        preexec_fn=os.setpgrp,
    )


#
# Run GDB with all the required scripts
#
def run_gdb(args):
    gdb_cmd = ["gdb"]

    for source_dir in args.dirs:
        gdb_cmd.append(f"--directory={source_dir}")

    # Create `coredump` command script
    coredump_script_path = create_coredump_gdb_script()

    # Include debug gdb commands (reset, rh, etc.)
    gdb_cmd += [
        "-x",
        f"{debug_scripts_path}/debug.gdb_cmds",
        "-x",
        coredump_script_path,
    ]

    # Add executable
    gdb_cmd.append(f"{args.elf}")

    if args.tui:
        gdb_cmd.append("-tui")

    subprocess.run(gdb_cmd)

    # Remove temporary coredump script file
    if coredump_script_path:
        os.remove(coredump_script_path)


parser = argparse.ArgumentParser()
parser.add_argument("elf", help="Firmware elf file")
parser.add_argument("dirs", help="Source search directories", nargs="+")

parser.add_argument("--tui", action="store_true", help="Use gdb-tui")
parser.add_argument(
    "--rtos", action="store_true", help="Enable RTOS support in openocd"
)

args, unknownargs = parser.parse_known_args()

load_dotenv(find_dotenv())

# Run openocd in the background
openocd = run_openocd(args)

# Wait for openocd to start up before trying to connect with gdb
time.sleep(1)

# Run GDB to do all the things!
run_gdb(args)

# Make sure we don't leave openocd running
openocd.terminate()
