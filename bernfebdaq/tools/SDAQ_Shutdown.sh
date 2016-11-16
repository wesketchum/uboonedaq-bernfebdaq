#!/bin/bash

source setup.sh

HOST=`hostname -s`

xmlrpc $HOST:5200/RPC2 pmt.stopSystem
ssh ${HOST} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
