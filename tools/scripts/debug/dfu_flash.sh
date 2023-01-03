#!/bin/bash

# TODO - check if there is a device in DFU mode (and try to enter it if not, once console is working)

dfu-util -d 0483:df11 -c 1 -i 0 -a 0 -s 0x8000000:leave -D $1
