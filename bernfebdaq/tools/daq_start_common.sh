#!/bin/bash

while getopts "n:p:r:" opt; do
    case "$opt" in

    n) 
       NODE="http://"
       NODE=$NODE$OPTARG
       ;;
    p) 
       PORT=$OPTARG
       ;;
    r) 
       RUNNUMBER=$OPTARG
       ;;
    esac
done

xmlrpc $NODE:$PORT/RPC2 daq.start $RUNNUMBER
