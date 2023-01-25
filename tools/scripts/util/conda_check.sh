#!/bin/bash

if [ -z "$CONDA_PREFIX" ]
then
	echo ""
	echo "*************************************"
    echo "*************************************"
    echo "*** CONDA ENVIRONMENT NOT ACTIVE! ***"
    echo "*************************************"
    echo "*************************************"
    echo ""
    exit 1
else
    exit 0
fi
