#!/usr/bin/bash

export ARTDAQ_PRODUCTS=/artdaq_products
export BASE_DIR=$PWD
export INSTALL_DIR=$BASE_DIR/install

export BASE_NAME=bernfebdaq

mkdir -p $INSTALL_DIR

if [[ $1 = "ubuntu" ]]; then
    echo $1
    export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu \
	UPS_OVERRIDE='-H Linux64bit+2.6-2.12' \
	CET_PLATINFO=Linux64bit+2.6-2.12:slf6:x86_64
fi

source $BASE_DIR/setup_common

echo ""
echo ""
echo "=============================================================================="
echo "To build now, do 'buildtool' ."
echo ""
echo ""
