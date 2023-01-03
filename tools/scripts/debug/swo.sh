#!/bin/bash

# TODO - support multiple tools (stlink/jlink/blackmagicprobe)

PROJ_BASE=`git rev-parse --show-toplevel`

TMPLOGFILE=`mktemp /tmp/swo_log.XXXXXXXXXX` || exit 1

set -m
openocd -f $PROJ_BASE/scripts/st_nucleo_l4.cfg -c "tpiu config internal $TMPLOGFILE uart off 80000000 1000000" -c "itm ports on" > /dev/null 2>&1 &
openocd_pid=$!
set +m

echo Logging SWO output to $TMPLOGFILE
echo Press Ctrl+C to stop

echo "running"
set -m
# Removing \x01 for stdout (still present in trace file)
tail -f $TMPLOGFILE | tr -d $'\x01'
set +m
echo "done"

kill -9 $openocd_pid
