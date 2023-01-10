#!/bin/bash

# TODO - support multiple tools (stlink/jlink/blackmagicprobe)

PROJ_ROOT=`git rev-parse --show-toplevel`
TOOLS_DIR=$PROJ_ROOT/tools

#
# Create gdb script with coredump command
# This needs to have user key info which is store in the .env file
# So we auto-generate this when running and delete after using
#

# Load .env fun
DOTENV=$PROJ_ROOT/.env
[ ! -f $DOTENV ] || export $(grep -v '^#' $DOTENV | xargs)

MEMFAULT_GDB_SCRIPT=$(mktemp /tmp/memfault.gdb_cmds.XXXXXX)

# Make sure we delete the memfault coredump scriptfile on exit
trap 'rm -f "$MEMFAULT_GDB_SCRIPT"' EXIT

# Create script file
echo "
define coredump
  source $PROJ_ROOT/src/third_party/memfault-firmware-sdk/scripts/memfault_gdb.py
  memfault login $MEMFAULT_USERNAME $MEMFAULT_PASSWORD -o sofar-ocean -p $MEMFAULT_PROJECT
  memfault coredump --region 0x20000000 0xC2000
end
" > $MEMFAULT_GDB_SCRIPT

#
# Start openOCD and connect to target
#
set -m
openocd -f $TOOLS_DIR/scripts/debug/stlink-mod.cfg -f $TOOLS_DIR/scripts/debug/st_nucleo_u5.cfg -f $TOOLS_DIR/scripts/debug/utils.cfg -c init > /dev/null 2>&1 &
openocd_pid=$!
set +m

sleep 1

#
# Connect over GDB!
#
# Don't take unknown args to be tacked on to the end of the gdb call
if [[ $5 == "-tui" ]]; then
  GDB_ARGS="-tui"
else
  GDB_ARGS=
fi

GDB_CMD="gdb"

# Add source directory as well as build directory to make sure we can find sources during debug
$GDB_CMD --directory=$2 --directory=$3 --directory=$4 -x $TOOLS_DIR/scripts/debug/debug.gdb_cmds -x $MEMFAULT_GDB_SCRIPT $1 $GDB_ARGS

# Only kill openOCD if it's still running
if ps -p $openocd_pid > /dev/null
then
kill -9 $openocd_pid
fi
