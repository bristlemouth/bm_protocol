#!/bin/bash

# TODO - support multiple tools (stlink/jlink/blackmagicprobe)

TOOLS_DIR=`git rev-parse --show-toplevel`/tools

set -m
openocd -f $TOOLS_DIR/scripts/debug/stlink-mod.cfg -f $TOOLS_DIR/scripts/debug/st_nucleo_l4_hardreset.cfg -f $TOOLS_DIR/scripts/debug/utils.cfg -c init -c reset -c halt > /dev/null 2>&1 &
openocd_pid=$!
set +m

sleep 1

# Don't take unknown args to be tacked on to the end of the gdb call
if [[ $5 == "-tui" ]]; then
  GDB_ARGS="-tui"
else
  GDB_ARGS=
fi

GDB_CMD="gdb"

# Add source directory as well as build directory to make sure we can find sources during debug
$GDB_CMD --directory=$2 --directory=$3 --directory=$4 -x $TOOLS_DIR/scripts/debug/debug.gdb_cmds $1 $GDB_ARGS

# Only kill openOCD if it's still running
if ps -p $openocd_pid > /dev/null
then
kill -9 $openocd_pid
fi
