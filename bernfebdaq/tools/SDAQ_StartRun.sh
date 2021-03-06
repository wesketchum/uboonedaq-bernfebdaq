#!/bin/bash

source setup.sh

while getopts "n:p:r:" opt; do
    case "$opt" in

    r) 
       RUNNUMBER=$OPTARG
       ;;
    esac
done

#aggregators first

#event builders next
daq_start_common.sh -n ubdaq-prod-crtevb-priv -p 5235 -r $RUNNUMBER

#board readers last
daq_start_common.sh -n ubdaq-prod-crt02-priv -p 5205 -r $RUNNUMBER
daq_start_common.sh -n ubdaq-prod-crt03-priv -p 5205 -r $RUNNUMBER
daq_start_common.sh -n ubdaq-prod-crt04-priv -p 5205 -r $RUNNUMBER

