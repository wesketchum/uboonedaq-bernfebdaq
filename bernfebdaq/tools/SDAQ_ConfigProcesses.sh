#!/bin/bash

source setup.sh

./daq_init_common.sh -n ubdaq-prod-crt02 -p 5205 -c BoardReader_BernDataZMQ_02.fcl
./daq_init_common.sh -n ubdaq-prod-crt03 -p 5205 -c BoardReader_BernDataZMQ_03.fcl
./daq_init_common.sh -n ubdaq-prod-crt04 -p 5205 -c BoardReader_BernDataZMQ_04.fcl
./daq_init_common.sh -n ubdaq-prod-crtevb -p 5235 -c EventBuilder_uBooNECRT.fcl


