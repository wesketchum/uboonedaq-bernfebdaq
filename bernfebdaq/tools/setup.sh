#!/bin/bash

source /artdaq_products/setup

#export CETPKG_INSTALL=/home/wketchum/uboonedaq-bernfebdaq/install
#export PRODUCTS=$CETPKG_INSTALL:$PRODUCTS

setup bernfebdaq v00_03_00 -qe10:eth:s41:prof

#export FHICL_FILE_PATH=/home/wketchum/uboonedaq-bernfebdaq/bernfebdaq/tools/fcl/:$FHICL_FILE_PATH