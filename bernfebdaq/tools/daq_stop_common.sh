#!/bin/bash

while getopts "n:p:" opt; do
    case "$opt" in

    n) 
       NODE="http://"
       NODE=$NODE$OPTARG
       ;;
    p) 
       PORT=$OPTARG
       ;;
    esac
done

xmlrpc $NODE:$PORT/RPC2 daq.stop
