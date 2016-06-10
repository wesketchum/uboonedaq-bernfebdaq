#!/usr/bin/bash

export ARTDAQ_PRODUCTS=/artdaq_products
export BASE_DIR=$PWD
export INSTALL_DIR=$BASE_DIR/install

mkdir -p $INSTALL_DIR

#export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu
export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu \
   UPS_OVERRIDE='-H Linux64bit+2.6-2.12' \
   CET_PLATINFO=Linux64bit+2.6-2.12:slf6:x86_64

source $BASE_DIR/bernfebdrv/setup/setupBERNFEBDRV

echo ""
echo ""
echo "=============================================================================="
echo "To build now, do 'buildtool' ."
echo ""
echo ""
