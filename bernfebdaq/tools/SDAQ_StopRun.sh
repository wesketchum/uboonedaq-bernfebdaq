#!/bin/bash

source setup.sh

#board readers first
daq_stop_common.sh -n ubdaq-prod-crt02-priv -p 5205 -r $RUNNUMBER
daq_stop_common.sh -n ubdaq-prod-crt03-priv -p 5205 -r $RUNNUMBER
daq_stop_common.sh -n ubdaq-prod-crt04-priv -p 5205 -r $RUNNUMBER

#event builders next
daq_stop_common.sh -n ubdaq-prod-crtevb-priv -p 5235 -r $RUNNUMBER

#aggregators last
