#!/bin/bash

while getopts "n:p:c:" opt; do
    case "$opt" in

    n) 
       NODE="http://"
       NODE=$NODE$OPTARG
       ;;
    p) 
       PORT=$OPTARG
       ;;
    c) 
       CONFIGFILE=$OPTARG
       ;;
    esac
done

echo "NODE = ${NODE}"
echo "PORT = ${PORT}"
echo "CONFIGFILE = ${CONFIGFILE}"

#CONFIG='"$(<$CONFIGFILE)"'
CONFIGDUMP=`fhicl-dump ${CONFIGFILE}`
#CONFIG="`strings ${CONFIGFILE}`"
#CONFIG=\'$CONFIG\'
#CONFIG="$(<$CONFIGFILE)"

CONFIG=`echo "$CONFIGDUMP" | sed 's/{/\\\\{/g' | sed 's/:/\\\\:/g' | sed 's/,/\\\\,/g' | sed 's/}/\\\\}/g'`

#echo $CONFIG

#xmlrpc $NODE:$PORT/RPC2 daq.init "$(<$CONFIGFILE)"
xmlrpc $NODE:$PORT/RPC2 daq.init "$CONFIG"
