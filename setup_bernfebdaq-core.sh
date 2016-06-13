#!/usr/bin/bash

export ARTDAQ_PRODUCTS=/artdaq_products
export BASE_DIR=$PWD
export INSTALL_DIR=$BASE_DIR/install

export BASE_NAME=bernfebdaq-core

mkdir -p $INSTALL_DIR

source $BASE_DIR/setup_common

echo ""
echo ""
echo "=============================================================================="
echo "To build now, do 'buildtool' ."
echo ""
echo ""
