#!/bin/bash

source setup.sh

HOST=(`hostname -s`)

while getopts "n:p:r:" opt; do
    case "$opt" in

    r) 
       RUNNUMBER=$OPTARG
       ;;
    esac
done

#aggregators first

#event builders next
./daq_start_common.sh -n ${HOST} -p 5235 -r $RUNNUMBER

#board readers last
./daq_start_common.sh -n ${HOST} -p 5205 -r $RUNNUMBER
