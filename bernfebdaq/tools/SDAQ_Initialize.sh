#!/bin/bash

source setup.sh

mkdir -p /tmp/pmt

pmt.rb -p 5200 -d pmtConfig --logpath /tmp --display ${DISPLAY} >& /tmp/pmt/pmt.log &
