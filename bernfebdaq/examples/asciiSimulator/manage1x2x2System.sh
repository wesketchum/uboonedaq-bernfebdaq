#!/bin/bash

source `which setupDemoEnvironment.sh` ""

AGGREGATOR_NODE=`hostname`
THIS_NODE=`hostname -s`

# this function expects a number of arguments:
#  1) the DAQ command to be sent
#  2) the run number (dummy for non-start commands)
#  3) the compression level for ADC data [0..2]
#  4) whether to run online monitoring
#  5) the data directory
#  6) the logfile name
#  7) whether to write data to disk [0,1]
#  8) the desired number of events in the run (stop command)
#  9) the desired duration of the run (minutes, stop command)
# 10) the desired size of each data file
# 11) the desired number of events in each file
# 12) the desired time duration of each file (minutes)
# 13) whether to print out CFG information (verbose)
function launch() {
  ebComp=$3
  agComp=$3
  if [[ "$3" == "2" ]]; then
    ebComp=1
    agComp=3
  elif [[ "$3" == "1" ]]; then
    ebComp=1
    agComp=0
  fi
  enableSerial=""
  if [[ "${13}" == "1" ]]; then
      enableSerial="-e"
  fi

  DemoControl.rb ${enableSerial} -s -c $1 \
	--ascii `hostname`,${ARTDAQDEMO_BR_PORT[0]},0 \
    --eb `hostname`,${ARTDAQDEMO_EB_PORT[0]},$ebComp \
    --eb `hostname`,${ARTDAQDEMO_EB_PORT[1]},$ebComp \
    --ag `hostname`,${ARTDAQDEMO_AG_PORT[0]},1,$agComp \
    --ag `hostname`,${ARTDAQDEMO_AG_PORT[1]},1,$agComp \
    --data-dir ${5} --online-monitoring $4 \
    --write-data ${7} --run-event-count ${8} \
    --run-duration ${9} --file-size ${10} \
    --file-event-count ${11} --file-duration ${12} \
    --run-number $2  2>&1 | tee -a ${6}
}

scriptName=`basename $0`
usage () {
    echo "
Usage: ${scriptName} [options] <command>
Where command is one of:
  init, start, pause, resume, stop, status, get-legal-commands,
  shutdown, start-system, restart, reinit, exit,
  fast-shutdown, fast-restart, fast-reinit, or fast-exit
General options:
  -h, --help: prints this usage message
Configuration options (init commands):
  -m <on|off>: specifies whether to run online monitoring [default=off]
  -D : disables the writing of data to disk
  -s <file size>: specifies the size threshold for closing data files (in MB)
      [default is 8000 MB (~7.8 GB); zero means that there is no file size limit]
  --file-events <count>: specifies the desired number of events in each file
      [default=0, which means no event count limit for files]
  --file-duration <duration>: specifies the desired duration of each file (minutes)
      [default=0, which means no duration limit for files]
  -c <compression level>: specifies the ADC data compression level
      0 = no compression
      1 = compression, both raw and compressed data kept [default]
      2 = compression, only compressed data kept
  -o <data dir>: specifies the directory for data files [default=/tmp]
Begin-run options (start command):
  -N <run number>: specifies the run number
End-run options (stop command):
  -n, --run-events <count>: specifies the desired number of events in the run
      [default=0, which ends the run immediately, independent of the number of events]
  -d, --run-duration <duration>: specifies the desired length of the run (minutes)
      [default=0, which ends the run immediately, independent of the duration of the run]
Notes:
  The start command expects a run number to be specified (-N).
  The primary commands are the following:
   * init - initializes (configures) the DAQ processes
   * start - starts a run
   * pause - pauses the run
   * resume - resumes the run
   * stop - stops the run
   * status - checks the status of each DAQ process
   * get-legal-commands - fetches the legal commands from each DAQ process
  Additional commands include:
   * shutdown - stops the run (if one is going), resets the DAQ processes
       to their ground state (if needed), and stops the MPI program (DAQ processes)
   * start-system - starts the MPI program (the DAQ processes)
   * restart - this is the same as a shutdown followed by a start-system
   * reinit - this is the same as a shutdown followed by a start-system and an init
   * exit - this resets the DAQ processes to their ground state, stops the MPI
       program, and exits PMT.
  Expert-level commands:
   * fast-shutdown - stops the MPI program (all DAQ processes) no matter what
       state they are in. This could have bad consequences if a run is going!
   * fast-restart - this is the same as a fast-shutdown followed by a start-system
   * fast-reinit - this is the same as a fast-shutdown followed by a start-system
       and an init
   * fast-exit - this stops the MPI program, and exits PMT.
Examples: ${scriptName} -p 32768 init
          ${scriptName} -N 101 start
" >&2
}

# parse the command-line options
originalCommand="$0 $*"
compressionLevel=1
onmonEnable=off
diskWriting=1
dataDir="/tmp"
runNumber=""
runEventCount=0
runDuration=0
fileSize=8000
fsChoiceSpecified=0
fileEventCount=0
fileDuration=0
verbose=0
OPTIND=1
while getopts "hc:N:o:m:Dn:d:s:w:v-:" opt; do
    if [ "$opt" = "-" ]; then
        opt=$OPTARG
    fi
    case $opt in
        h | help)
            usage
            exit 1
            ;;
        c)
            compressionLevel=${OPTARG}
            ;;
        N)
            runNumber=${OPTARG}
            ;;
        m)
            onmonEnable=${OPTARG}
            ;;
        o)
            dataDir=${OPTARG}
            ;;
        D)
            diskWriting=0
            ;;
        n)
            runEventCount=${OPTARG}
            ;;
        d)
            runDuration=${OPTARG}
            ;;
        run-events)
            runEventCount=${!OPTIND}
            let OPTIND=$OPTIND+1
            ;;
        run-duration)
            runDuration=${!OPTIND}
            let OPTIND=$OPTIND+1
            ;;
        file-events)
            fileEventCount=${!OPTIND}
            let OPTIND=$OPTIND+1
            ;;
        file-duration)
            fileDuration=${!OPTIND}
            let OPTIND=$OPTIND+1
            ;;
        s)
            fileSize=${OPTARG}
            fsChoiceSpecified=1
            ;;
        v)
            verbose=1
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done
shift $(($OPTIND - 1))

# fetch the command to run
if [ $# -lt 1 ]; then
    usage
    exit
fi
command=$1
shift

# verify that the command is one that we expect
if [[ "$command" != "start-system" ]] && \
   [[ "$command" != "init" ]] && \
   [[ "$command" != "start" ]] && \
   [[ "$command" != "pause" ]] && \
   [[ "$command" != "resume" ]] && \
   [[ "$command" != "stop" ]] && \
   [[ "$command" != "status" ]] && \
   [[ "$command" != "get-legal-commands" ]] && \
   [[ "$command" != "shutdown" ]] && \
   [[ "$command" != "fast-shutdown" ]] && \
   [[ "$command" != "restart" ]] && \
   [[ "$command" != "fast-restart" ]] && \
   [[ "$command" != "reinit" ]] && \
   [[ "$command" != "fast-reinit" ]] && \
   [[ "$command" != "exit" ]] && \
   [[ "$command" != "fast-exit" ]]; then
    echo "Invalid command."
    usage
    exit
fi

# verify that the expected arguments were provided
if [[ "$command" == "start" ]] && [[ "$runNumber" == "" ]]; then
    echo ""
    echo "*** A run number needs to be specified."
    usage
    exit
fi

# fill in values for options that weren't specified
if [[ "$runNumber" == "" ]]; then
    runNumber=101
fi

# translate the onmon enable flag
if [[ "$onmonEnable" == "on" ]]; then
    onmonEnable=1
else
    onmonEnable=0
fi

# verify that we don't have both a number of events and a time limit
# for stopping a run
if [[ "$command" == "stop" ]]; then
    if [[ "$runEventCount" != "0" ]] && [[ "$runDuration" != "0" ]]; then
        echo ""
        echo "*** Only one of event count or run duration may be specified."
        usage
        exit
    fi
fi

# build the logfile name
TIMESTAMP=`date '+%Y%m%d%H%M%S'`
logFile="/tmp/masterControl/dsMC-${TIMESTAMP}-${command}.log"
echo "${originalCommand}" > $logFile
echo ">>> ${originalCommand} (Disk writing is ${diskWriting})"

# calculate the shmkey that should be checked
let shmKey=1078394880+${ARTDAQDEMO_PMT_PORT}
shmKeyString=`printf "0x%x" ${shmKey}`

# invoke the requested command
if [[ "$command" == "shutdown" ]]; then
    # first send a stop command to end the run (in case it is needed)
    launch "stop" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
    # next send a shutdown command to move the processes to their ground state
    launch "shutdown" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
    # stop the MPI program
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    # clean up any stale shared memory segment
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
elif [[ "$command" == "start-system" ]]; then
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.startSystem
elif [[ "$command" == "restart" ]]; then
    # first send a stop command to end the run (in case it is needed)
    launch "stop" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
    # next send a shutdown command to move the processes to their ground state
    launch "shutdown" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
    # stop the MPI program
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    # clean up any stale shared memory segment
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    # start the MPI program
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.startSystem
elif [[ "$command" == "reinit" ]]; then
    # first send a stop command to end the run (in case it is needed)
    launch "stop" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
    # next send a shutdown command to move the processes to their ground state
    launch "shutdown" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
    # stop the MPI program
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    # clean up any stale shared memory segment
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    # start the MPI program
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.startSystem
    # send the init command to re-initialize the system
    sleep 5
    launch "init" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
elif [[ "$command" == "exit" ]]; then
    launch "shutdown" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.exit
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"

elif [[ "$command" == "fast-shutdown" ]]; then
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
elif [[ "$command" == "fast-restart" ]]; then
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.startSystem
elif [[ "$command" == "fast-reinit" ]]; then
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.startSystem
    sleep 5
    launch "init" $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
elif [[ "$command" == "fast-exit" ]]; then
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.stopSystem
    xmlrpc ${THIS_NODE}:${ARTDAQDEMO_PMT_PORT}/RPC2 pmt.exit
    ssh ${AGGREGATOR_NODE} "ipcs | grep ${shmKeyString} | awk '{print \$2}' | xargs ipcrm -m 2>/dev/null"

else
    launch $command $runNumber $compressionLevel $onmonEnable $dataDir \
        $logFile $diskWriting $runEventCount $runDuration $fileSize \
        $fileEventCount $fileDuration $verbose
fi
