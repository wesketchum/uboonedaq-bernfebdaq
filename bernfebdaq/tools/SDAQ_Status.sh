#!/bin/bash

source setup.sh

HOST=(`hostname -s`)

echo "STATUS: BoardReader:"
./daq_status_common.sh -n ${HOST} -p 5205
echo "------------------------------------------------"

echo "STATUS: EventBuilder:"
./daq_status_common.sh -n ${HOST} -p 5235
echo "------------------------------------------------"
echo STATUS COMPLETE

echo "------------------------------------------------"
