#!/bin/bash

source setup.sh

HOST=(`hostname -s`)

#board readers first
./daq_stop_common.sh -n ${HOST} -p 5205

#event builders next
./daq_stop_common.sh -n ${HOST} -p 5235

#aggregators last
