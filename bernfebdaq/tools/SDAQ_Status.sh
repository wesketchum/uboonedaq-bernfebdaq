#!/bin/bash

source setup.sh

echo "STATUS: BoardReaders:"
daq_status_common.sh -n ubdaq-prod-crt02-priv -p 5205
daq_status_common.sh -n ubdaq-prod-crt03-priv -p 5205
daq_status_common.sh -n ubdaq-prod-crt04-priv -p 5205
echo "------------------------------------------------"

echo "STATUS: EventBuilder(s):"
daq_status_common.sh -n ubdaq-prod-crtevb-priv -p 5235
echo "------------------------------------------------"
echo STATUS COMPLETE

echo "------------------------------------------------"
