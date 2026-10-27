#!/bin/bash

cd preset-builds/bootloader

make flash 2>&1 | grep 'Verified OK'

sleep 1
cd ../../cmake-build/motion_module

make flash 2>&1 | grep 'Verified OK'

sleep 2

pyserial-miniterm
