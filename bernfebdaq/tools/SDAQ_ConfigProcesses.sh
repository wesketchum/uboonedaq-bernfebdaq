#!/bin/bash

source setup.sh

HOST=(`hostname -s`)

./daq_init_common.sh -n ${HOST} -p 5205 -c BoardReader_BernDataZMQ.fcl
./daq_init_common.sh -n ${HOST} -p 5235 -c EventBuilder_uBooNECRT.fcl


