#!/bin/sh

PROJ_BASE=`git rev-parse --show-toplevel`

CDDL_DIR=$PROJ_BASE/tools/scripts/cddl
ZCBOR_DIR=$PROJ_BASE/src/third_party/zcbor

# Convert ROS2 to CDDL file
BM_TYPES=$(python $CDDL_DIR/bm_msg_main.py -i $CDDL_DIR/msgs -o $CDDL_DIR/msgs -p bm -f bm_out.cddl)
echo $BM_TYPES

# Generate .c/.h from generated CDDL file
python3 $ZCBOR_DIR/zcbor/zcbor.py code -c $CDDL_DIR/msgs/bm_out.cddl -e -d -t $BM_TYPES --oc bm_zcbor.c --oh bm_zcbor.h

# Move generated files to bristlemouth application 
mv *.c $PROJ_BASE/src/lib/bcl/src
mv *.h $PROJ_BASE/src/lib/bcl/include
