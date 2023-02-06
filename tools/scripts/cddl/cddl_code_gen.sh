#!/bin/sh

echo ${PWD}

# Convert ROS2 to CDDL file
BM_TYPES=$(python3 ../../tools/scripts/cddl/bm_msg_main.py -i ../../tools/scripts/cddl/msgs -o ../../tools/scripts/cddl/msgs -p bm -f bm_out.cddl)

# Generate .c/.h from generated CDDL file
python3 ../../src/third_party/zcbor/zcbor/zcbor.py code -c ../../tools/scripts/cddl/msgs/bm_out.cddl -e -d -t $BM_TYPES --oc bm_zcbor.c --oh bm_zcbor.h

# Move generated files to bristlemouth application 
mv *.c ../../src/lib/bcl/src
mv *.h ../../src/lib/bcl/include
