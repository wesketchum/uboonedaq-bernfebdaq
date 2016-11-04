#!/bin/bash

source setup.sh

HOST=`hostname -s`

xmlrpc $HOST:5200/RPC2 pmt.startSystem
