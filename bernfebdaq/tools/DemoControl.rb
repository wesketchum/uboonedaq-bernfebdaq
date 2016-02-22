#!/usr/bin/env ruby

require 'date'
require "optparse"
require "ostruct"
require "xmlrpc/client"
require "rexml/document"

# To summarize this script in one sentence, it is designed to send
# basic DAQ transition commands provided at the command line (init,
# start, stop) to running artdaq processes. The most complex of these
# commands is "init", which involves taking arguments supplied at the
# command line and using them to create FHiCL configuration scripts
# read in by the processes. Note that the actual program flow (i.e.,
# the equivalent of C/C++'s "main") begins at the bottom of this
# script, at the line "if __FILE__ == $0"; when first examining this
# script it's a good idea to begin there.


require File.join( File.dirname(__FILE__), 'demo_utilities' )

# The following includes bring in ruby functions which, given a set of
# arguments, will generate FHiCL code usable by the artdaq processes
# (BoardReaderMain, EventBuilderMain, AggregatorMain)

require File.join( File.dirname(__FILE__), 'generateToy' )
require File.join( File.dirname(__FILE__), 'generatePattern' )
require File.join( File.dirname(__FILE__), 'generateNormal' )
require File.join( File.dirname(__FILE__), 'generateV1720' )
require File.join( File.dirname(__FILE__), 'generateAscii' )
require File.join( File.dirname(__FILE__), 'generateUDP' )
require File.join( File.dirname(__FILE__), 'generateWFViewer' )

require File.join( File.dirname(__FILE__), 'generateBoardReaderMain' )
require File.join( File.dirname(__FILE__), 'generateEventBuilderMain' )
require File.join( File.dirname(__FILE__), 'generateAggregatorMain' )



# PLEASE NOTE: If/when there comes a time that we want to add more board
# types to this script, we should go back to the ds50MasterControl script
# and use it as an example since it may not be obvious how to add boards
# from what currently exists in this script.  KAB, 28-Dec-2013

# [SCF] It'd be nice if we were using a newer version of ruby (1.9.3) that had
# better string format substitution.  For the time being we'll setup our
# string constants using the newer convention and do the ugly substitutions.

# 17-Sep-2013, KAB - provide a way to fetch the online monitoring
# configuration from a separate file
if ENV['ARTDAQ_CONFIG_PATH']
  @cfgPathArray = ENV['ARTDAQ_CONFIG_PATH'].split(':')
  @cfgPathArray = @cfgPathArray.reverse
  (@cfgPathArray).each do |path|
    $LOAD_PATH.unshift path
  end
end
begin
  require 'onmon_config'
rescue Exception => msg
end

# And, this assignment for the prescale will only
# be used if it wasn't included from the external file already
if (defined?(ONMON_EVENT_PRESCALE)).nil? || (ONMON_EVENT_PRESCALE).nil?
  ONMON_EVENT_PRESCALE = 1
end
# ditto, the online monitoring modules that are run
if (defined?(ONMON_MODULES)).nil? || (ONMON_MODULES).nil?
  ONMON_MODULES = "[ app, wf ]"
end

# John F., 2/5/14

# ConfigGen, a class designed to generate the FHiCL configuration
# scripts which configure the artdaq processes, has had many of its
# functions moved into separate *.rb files, the logic being that
# there's nothing specific to this control script about a function
# which generates FHiCL configuration scripts for components which can
# be used in multiple contexts (e.g., V172x simulators).


class ConfigGen

  def generateComposite(totalEBs, totalFRs, configStringArray)


    compositeConfig = String.new( "\
%{prolog}
daq: {
  max_fragment_size_words: %{size_words}
  fragment_receiver: {
    mpi_buffer_count: %{buffer_count}
    first_event_builder_rank: %{total_frs}
    event_builder_count: %{total_ebs}
    generator: CompositeDriver
    fragment_id: 999
    board_id: 999
    generator_config_list:
    [
      # the format of this list is {daq:<paramSet},{daq:<paramSet>},...
      %{generator_list}
    ]
  }
}" )

    compositeConfig.gsub!(/\%\{total_ebs\}/, String(totalEBs))
    compositeConfig.gsub!(/\%\{total_frs\}/, String(totalFRs))
    compositeConfig.gsub!(/\%\{buffer_count\}/, String(totalEBs*8))

    # The complications here are A) determining the largest buffer size that
    # has been requested by the child configurations and using that for the
    # composite buffers size and B) moving any PROLOG declarations from the
    # individual configuration strings to the front of the full CFG string.
    prologList = []
    fragSizeWords = 0
    configString = ""
    first = true
    configStringArray.each do |cfg|
      my_match = /(.*)BEGIN_PROLOG(.*)END_PROLOG(.*)/im.match(cfg)
      if my_match
        thisProlog = my_match[2]
        cfg = my_match[1] + my_match[3]

        found = false
        prologList.each do |savedProlog|
          if thisProlog == savedProlog
            found = true
            break
          end
        end
        if ! found
          prologList << thisProlog
        end
      end

      if first
        first = false
      else
        configString += ", "
      end
      configString += "{" + cfg + "}"

      my_match = /max_fragment_size_words\s*\:\s*(\d+)/.match(cfg)
      if my_match
        begin
          sizeWords = Integer(my_match[1])
          if sizeWords > fragSizeWords
            fragSizeWords = sizeWords
          end
        rescue Exception => msg
          puts "Warning: exception parsing size_words in composite child configuration: " + my_match[1] + " " + msg
        end
      end
    end
    compositeConfig.gsub!(/\%\{size_words\}/, String(fragSizeWords))
    compositeConfig.gsub!(/\%\{generator_list\}/, configString)

    prologString = ""
    if prologList.length > 0
      prologList.each do |savedProlog|
        prologString += "\n"
        prologString += savedProlog
      end
      prologString = "BEGIN_PROLOG" + prologString + "\nEND_PROLOG"
    end
    compositeConfig.gsub!(/\%\{prolog\}/, prologString)

    return compositeConfig
  end

  def generateXmlRpcClientList(cmdLineOptions)
    xmlrpcClients = ""
#    boardreaders = Array.new + @options.v1720s + @options.toys
#    boardreaders.each do |proc|
    (cmdLineOptions.v1720s + cmdLineOptions.toys + cmdLineOptions.asciis + cmdLineOptions.udps).each do |proc|
      br = cmdLineOptions.boardReaders[proc.board_reader_index]
      if br.hasBeenIncludedInXMLRPCList
        next
      else
        br.hasBeenIncludedInXMLRPCList = true
        xmlrpcClients += ";http://" + proc.host + ":" +
          String(proc.port) + "/RPC2"
        xmlrpcClients += ",3"  # group number
      end
    end
    (cmdLineOptions.eventBuilders).each do |proc|
      xmlrpcClients += ";http://" + proc.host + ":" +
        String(proc.port) + "/RPC2"
      xmlrpcClients += ",4"  # group number
    end
    (cmdLineOptions.aggregators).each do |proc|
      xmlrpcClients += ";http://" + proc.host + ":" +
        String(proc.port) + "/RPC2"
      xmlrpcClients += ",5"  # group number
    end
    return xmlrpcClients
  end
end

# As its name would suggest, "CommandLineParser" is a class designed
# to take the arguments passed to the script and to store them in the
# "options" member structure; the information in this structure will
# in turn be used to direct the generation of FHiCL configuration
# scripts used to control the artdaq processes

class CommandLineParser
  def initialize(configGen)
    @configGen = configGen
    @options = OpenStruct.new
    @options.aggregators = []
    @options.eventBuilders = []
    @options.v1720s = []
    @options.toys = []
    @options.asciis = []
    @options.pbrs = []
    @options.udps = []
    @options.boardReaders = []
    @options.dataDir = nil
    @options.command = nil
    @options.summary = false
    @options.runNumber = "0101"
    @options.serialize = false
    @options.runOnmon = 0
    @options.onmonFile = nil
    @options.onmon_modules = nil
    @options.writeData = 1
    @options.runDurationSeconds = -1
    @options.eventsInRun = -1
    @options.fileSizeThreshold = 0
    @options.fileDurationSeconds = 0
    @options.eventsInFile = 0
    @options.onmonFileEnabled = 0

    @optParser = OptionParser.new do |opts|
      opts.banner = "Usage: DemoControl.rb [options]"
      opts.separator ""
      opts.separator "Specific options:"

      @options.serialize = true
      opts.on("-C", "--config-file [file name]",
              "ARTDAQ-configuration XML Configuration file") do |configFile|
        puts "Configuration File is " + configFile
        doc = REXML::Document.new(File.new(configFile)).root
        puts "This configuration brought to you by " + doc.elements["author"].text
      
        portNumber = ENV['ARTDAQ_BASE_PORT'].to_i
 
        if doc.elements["dataLogger/enabled"].text == "true"
          @options.writeData = "1"
        else
          @options.writeData = "0"
        end
        if(doc.elements["onlineMonitor/enabled"].text == "true")
          @options.onmonFileEnabled = 1
        else
          @options.onmonFileEnabled = 0
        end
        if(doc.elements["onlineMonitor/viewerEnabled"].text == "true")
          @options.onmon_modules = "[ app, wf ]"
        else
          @options.onmon_modules = "[wf]"
        end
        

        dlConfig = OpenStruct.new
        dlConfig.host = doc.elements["dataLogger/hostname"].text
        dlConfig.port = portNumber + 1
        dlConfig.kind = "ag"
        dlConfig.bunch_size = 1
        dlConfig.compression_level = 0
        dlConfig.demoPrescale =0
        dlConfig.index = 0
        @options.aggregators << dlConfig
        omConfig = OpenStruct.new
        omConfig.host = doc.elements["onlineMonitor/hostname"].text
        omConfig.port = portNumber + 2
        omConfig.kind = "ag"
        omConfig.bunch_size = 1
        omConfig.compression_level = 0
        omConfig.demoPrescale = 0
        omConfig.index = 1
        @options.aggregators << omConfig


        # Board Readers
        currentPort = portNumber + 3
        if doc.elements["boardReaders"] != nil
          doc.elements["boardReaders"].each() { |element| 
            begin
              if element.elements["enabled"].text == "true"
                puts "DEBUG: BR Enabled"
                brConfig = OpenStruct.new
                brConfig.host = element.elements["hostname"].text
                brConfig.port = currentPort
                brConfig.board_id = @options.boardReaders.length
                currentPort += 1
                puts "DEBUG: Host and Port Setup"
                brConfig.fragType = element.elements["type"].text
                brConfig.name = element.elements["name"].text
                brConfig.eventSize = nil
                puts "DEBUG: Before getting index"
                brConfig.index = (@options.v1720s + @options.toys + @options.asciis + @options.pbrs).length
                brConfig.kind = "pbr"
                puts "DEBUG: Loading TypeConfig"
                typeConfig = ""
                element.elements["typeConfig"].each() { |config|
                   begin
                   if config.name == "generator_id"
                     brConfig.generator_id = config.text
                   else
                     typeConfig += config.name + ": " + config.text + "\n"
                   end
                   rescue
                     puts "DEBUG: RESCUE in TypeConfig"
                   end
                }
                brConfig.typeConfig = typeConfig
                brConfig.board_reader_index = addToBoardReaderList(brConfig.host, brConfig.port, brConfig.fragType,
                                                                   brConfig.index, brConfig.eventSize, true)
                puts "DEBUG: BR Config Complete"
                @options.pbrs << brConfig
              end
            rescue
               puts "DEBUG: RESCUE in BR Config"
            end
          }
        end

        # Event Builders
        numEvbs = doc.elements["eventBuilders/count"].text
        compression = doc.elements["eventBuilders/compress"].text == "true" ? 1 : 0
        *hosts = doc.elements["eventBuilders/hostnames/hostname"]     
        it = 0
        while it < numEvbs.to_i do
          ebConfig = OpenStruct.new
          ebConfig.host = hosts.at(it % hosts.size).text
          ebConfig.port = currentPort
          currentPort += 1
          ebConfig.compression_level = compression
          ebConfig.kind = "eb"
          ebConfig.index = it
          @options.eventBuilders << ebConfig
          it += 1
        end

        @options.dataDir = doc.elements["dataDir"].text
        if doc.elements["onlineMonitor/enabled"].text == "true"
          @options.runOnmon = "1"
        else
          @options.runOnmon = "0"
        end
        runMode = doc.elements["dataLogger/runMode"].text
        if(runMode == "Time")
        @options.runDurationSeconds = doc.elements["dataLogger/runValue"].text.to_i
        elsif(runMode == "Events")
        @options.eventsInRun = doc.elements["dataLogger/runValue"].text.to_i
        end
        fileMode = doc.elements["dataLogger/fileMode"].text
        if(fileMode == "Size")
        @options.fileSizeThreshold = doc.elements["dataLogger/fileValue"].text.to_i
        elsif (fileMode == "Time")
        @options.fileDurationSeconds = doc.elements["dataLogger/fileValue"].text.to_i
        elsif (fileMode == "Events")
        @options.eventsInFile = doc.elements["dataLogger/fileValue"].text.to_i
        end
      end

      opts.on("--eb [host,port,compression_level]", Array,
              "Add an event builder that runs on the",
              "specified host and port and optionally",
              "compresses ADC data.") do |eb|
        if eb.length != 3
          puts "You must specifiy a host, port, and compression level."
          exit
        end
        ebConfig = OpenStruct.new
        ebConfig.host = eb[0]
        ebConfig.port = Integer(eb[1])
        ebConfig.compression_level = Integer(eb[2])
        ebConfig.kind = "eb"
        ebConfig.index = @options.eventBuilders.length
        @options.eventBuilders << ebConfig
      end

      opts.on("--ag [host,port,bunch_size,compression_level,demoPrescale]", Array,
              "Add an aggregator that runs on the",
              "specified host and port.  Also specify the",
              "number of events to pass to art per bunch,",
              "and the compression level.") do |ag|
        if ag.length < 4
          puts "You must specifiy a host, port, bunch size, and"
          puts "compression level."
          exit
        end
        agConfig = OpenStruct.new
        agConfig.host = ag[0]
        agConfig.port = Integer(ag[1])
        agConfig.kind = "ag"
        agConfig.bunch_size = Integer(ag[2])
        agConfig.compression_level = Integer(ag[3])
        if ag.length == 5
            agConfig.demoPrescale = Integer(ag[4])
        else
            agConfig.demoPrescale = 0
        end
        agConfig.index = @options.aggregators.length
        @options.aggregators << agConfig
      end
    
      opts.on("--v1720 [host,port,board_id]", Array, 
              "Add a V1720 fragment receiver that runs on the specified host and port, ",
              "and has the specified board ID.") do |v1720|
        if v1720.length != 3
          puts "You must specifiy a host, port, and board ID."
          exit
        end
        v1720Config = OpenStruct.new
        v1720Config.host = v1720[0]
        v1720Config.port = Integer(v1720[1])
        v1720Config.board_id = Integer(v1720[2])
        v1720Config.kind = "V1720"
        v1720Config.fragType = "V1720"
        v1720Config.index = (@options.v1720s + @options.toys + @options.asciis + @options.pbrs + @options.udps).length
        v1720Config.board_reader_index = addToBoardReaderList(v1720Config.host, v1720Config.port,
                                                              v1720Config.kind, v1720Config.index)
        @options.v1720s << v1720Config
      end

      opts.on("--v1724 [host,port,board_id]", Array, 
              "Add a V1724 fragment receiver that runs on the specified host, port, ",
              "and board ID.") do |v1724|
        if v1724.length != 3
          puts "You must specifiy a host, port, and board ID."
          exit
        end
        v1724Config = OpenStruct.new
        v1724Config.host = v1724[0]
        v1724Config.port = Integer(v1724[1])
        v1724Config.board_id = Integer(v1724[2])
        v1724Config.kind = "V1724"
        v1724Config.fragType = "V1724"
        v1724Config.index = (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).length
        v1724Config.board_reader_index = addToBoardReaderList(v1724Config.host, v1724Config.port,
                                                              v1724Config.kind, v1724Config.index)
        # NOTE that we're simply adding this to the 1720 list...
        @options.v1720s << v1724Config
      end

      opts.on("--ascii [host,port,board_id,<string1>,<string2>]", Array, 
              "Add a TOY1 fragment receiver that runs on the specified host, port, ",
              "and board ID. Generates alternating string1 and string2's.") do |ascii|
        if ascii.length < 3
          puts "You must specifiy a host, port, and board ID."
          exit
        end
        asciiConfig = OpenStruct.new
        asciiConfig.host = ascii[0]
        asciiConfig.port = Integer(ascii[1])
        asciiConfig.board_id = Integer(ascii[2])
        asciiConfig.kind = "ASCII"
        asciiConfig.fragType = "ASCII"
        if ascii.length == 5
          asciiConfig.string1 = ascii[3]
          asciiConfig.string2 = ascii[4]
        end
        asciiConfig.index = (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).length
        asciiConfig.board_reader_index = addToBoardReaderList(asciiConfig.host, asciiConfig.port,
                                                              asciiConfig.kind, asciiConfig.index)
        @options.asciis << asciiConfig
      end

      opts.on("--toy1 [host,port,board_id,<eventSize>,<generator_id>]", Array, 
              "Add a TOY1 fragment receiver that runs on the specified host, port, ",
              "and board ID. Generates events of size eventSize bytes. ", 
              "Generator ID must be one of: Uniform, Normal, or Pattern") do |toy1|
        if toy1.length < 3
          puts "You must specifiy a host, port, and board ID."
          exit
        end
        toy1Config = OpenStruct.new
        toy1Config.host = toy1[0]
        toy1Config.port = Integer(toy1[1])
        toy1Config.board_id = Integer(toy1[2])
        toy1Config.kind = "TOY1"
        toy1Config.fragType = "TOY1"
        toy1Config.generator_id = "Uniform"
        if toy1.length == 5
          toy1Config.generator_id = toy1[4]
        end
        toy1Config.eventSize = nil
        if toy1.length > 3 && toy1[3] != "na"
          toy1Config.eventSize = Integer(toy1[3])
        end
        toy1Config.index = (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).length
        toy1Config.board_reader_index = addToBoardReaderList(toy1Config.host, toy1Config.port,
                                                              toy1Config.kind, toy1Config.index, toy1Config.eventSize)
        @options.toys << toy1Config
      end


      opts.on("--toy2 [host,port,board_id,<eventSize>,<generator_id>]", Array, 
              "Add a TOY2 fragment receiver that runs on the specified host, port, ",
              "and board ID. Generates events of size eventSize bytes. ",
              "Generator ID must be one of: Uniform, Normal, or Pattern") do |toy2|
        if toy2.length < 3
          puts "You must specifiy a host, port, and board ID."
          exit
        end
        toy2Config = OpenStruct.new
        toy2Config.host = toy2[0]
        toy2Config.port = Integer(toy2[1])
        toy2Config.board_id = Integer(toy2[2])
        toy2Config.kind = "TOY2"
        toy2Config.fragType = "TOY2"
        toy2Config.generator_id = "Uniform"
        if toy2.length == 5
          toy2Config.generator_id = toy2[4]
        end
        toy2Config.eventSize = nil
        if toy2.length > 3 && toy2[3] != "na"
          toy2Config.eventSize = Integer(toy2[3])
        end
        toy2Config.index = (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).length
        toy2Config.board_reader_index = addToBoardReaderList(toy2Config.host, toy2Config.port,
                                                              toy2Config.kind, toy2Config.index, toy2Config.eventSize)

        @options.toys << toy2Config
      end

      opts.on("--udp [host,port,board_id,udp_host,udp_port]", Array, 
              "Add a UDP fragment receiver that runs on the specified host, port, ",
              "and board ID. It will connect to a data source at udp_host:udp_port.") do |udp|
        if udp.length < 5
          puts "You must specifiy a host, port, and board ID."
          exit
        end
        udpConfig = OpenStruct.new
        udpConfig.host = udp[0]
        udpConfig.port = Integer(udp[1])
        udpConfig.board_id = Integer(udp[2])
        udpConfig.kind = "UDP"
        udpConfig.udp_host = udp[3]
        udpConfig.udp_port = Integer(udp[4])
        udpConfig.index = (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).length
        udpConfig.board_reader_index = addToBoardReaderList(udpConfig.host, udpConfig.port,
                                                              udpConfig.kind, udpConfig.index)

        @options.udps << udpConfig
      end

      opts.on("-d", "--data-dir [data dir]", 
              "Directory that the event builders will", "write data to.") do |dataDir|
        @options.dataDir = dataDir
      end

      opts.on("-m", "--online-monitoring [enabled,file_enabled,file_path]", Array,
              "Whether to run the online monitoring modules,", 
              "also whether and whither to send file output from the online monitoring" ) do |runOnmon|
        @options.runOnmon = Integer(runOnmon[0])
        @options.onmonFileEnabled = Integer(runOnmon[1])
        @options.onmonFile = runOnmon[2]
      end

      opts.on("--onmon-file [file_path]",
              "File name for onmon output") do |onmonFile|
        @options.onmonFileEnabled = 1
        @options.onmonFile = onmonFile
      end

      opts.on("-w", "--write-data [enable flag (0 or 1)]", 
              "Whether to write data to disk.") do |writeData|
        @options.writeData = writeData
      end

      opts.on("-c", "--command [command]", 
              "Execute a command: start, stop, init, shutdown, pause, resume, status, get-legal-commands.") do |command|
        @options.command = command
      end

      opts.on("-r", "--run-number [number]", "Specify the run number.") do |run|
        @options.runNumber = run
      end

      opts.on("-t", "--run-duration [minutes]",
              "Stop the run after the specified amount of time (minutes).") do |timeInMinutes|
        @options.runDurationSeconds = Integer(timeInMinutes) * 60
      end

      opts.on("-n", "--run-event-count [number]",
              "Stop the run after the specified number of events have been collected.") do |eventCount|
        @options.eventsInRun = Integer(eventCount)
      end

      opts.on("-f", "--file-size [number of MB]",
              "Close each data file when the specified size is reached (MB).") do |fileSize|
        @options.fileSizeThreshold = Float(fileSize)
      end

      opts.on("--file-duration [minutes]",
              "Closes each file after the specified amount of time (minutes).") do |timeInMinutes|
        @options.fileDurationSeconds = Integer(timeInMinutes) * 60
      end

      opts.on("--file-event-count [number]",
              "Close each file after the specified number of events have been written.") do |eventCount|
        @options.eventsInFile = Integer(eventCount)
      end

      opts.on("-s", "--summary", "Summarize the configuration.") do
        @options.summary = true
      end

      opts.on("-e", "--serialize", "Serialize the config for System Control.") do
        @options.serialize = true
      end

      opts.on_tail("-h", "--help", "Show this message.") do
        puts opts
        exit
      end
    end
  end

  def parse()
    if ARGV.length == 0
      puts @optParser
      exit
    end

    @optParser.parse(ARGV)
    self.validate()
  end

  def validate()
    # In sim mode make sure there is an eb and a 1720
    return nil
  end

  def addToBoardReaderList(host, port, kind, boardIndex, eventSize = nil, isPBR = nil)
    # check for an existing boardReader with the same host and port
    brIndex = 0
    @options.boardReaders.each do |br|
      if host == br.host && port == br.port
        if isPBR != nil
          br.kindList << "pbr"
        else
          br.kindList << kind
        end
        br.boardIndexList << boardIndex
        br.cfgList << ""
        br.boardCount += 1
        return brIndex
      end
      brIndex += 1
    end

    # if needed, create a new boardReader
    br = OpenStruct.new
    br.host = host
    br.port = port
    if isPBR != nil
      br.kindList = ["pbr"]
    else
      br.kindList = [kind]
    end
    br.boardIndexList = [boardIndex]
    br.cfgList = [""]
    br.boardCount = 1
    br.commandHasBeenSent = false
    br.hasBeenIncludedInXMLRPCList = false
    br.kind = "multi-board"
    br.eventSize = eventSize
    brIndex = @options.boardReaders.length
    @options.boardReaders << br
    return brIndex
  end

  def summarize()
    # Print out a summary of the configuration that was passed in from the
    # the command line.  Everything will be printed in terms of what process
    # is running on which host.
    puts "Configuration Summary:"
    hostMap = {}
    (@options.eventBuilders + @options.v1720s + @options.toys + @options.aggregators + @options.asciis + @options.udps + @options.pbrs).each do |proc|
      if not hostMap.keys.include?(proc.host)
        hostMap[proc.host] = []
      end
      hostMap[proc.host] << proc
    end

    hostMap.each_key { |host|
      puts "  %s:" % [host]
      hostMap[host].sort! { |x,y|
        x.port <=> y.port
      }

      totalEBs = @options.eventBuilders.length
      totalFRs = @options.boardReaders.length
      hostMap[host].each do |item|
        case item.kind
        when "eb"
          puts "    EventBuilder, port %d, rank %d" % [item.port, totalFRs + item.index]
        when "ag"
          puts "    Aggregator, port %d, rank %d" % [item.port, totalEBs + totalFRs + item.index]
        when "pbr"
          puts "    BoardReader, port %d, rank %d, board_id %d, generator %s" %
            [ item.port, item.index, item.board_id, item.fragType ]
        when "V1720", "V1724", "TOY1", "TOY2", "ASCII"
          puts "    FragmentReceiver, Simulated %s, port %d, rank %d, board_id %d" % 
            [item.kind.upcase,
             item.port,
             item.index,
             item.board_id]
        when "UDP"
          puts "    FragmentGenerator, UDPReceiver, port %d, rank %d, board_id %d" %
             [ item.port, item.index, item.board_id ]
        end
      end
      puts ""
    }
    STDOUT.flush
    return nil
  end

  def getOptions()
    return @options
  end
end

# "SystemControl" is a class whose member functions bear a 1-to-1
# correspondence with the standard commands which can be passed to
# this script: init, start, etc. As you might expect, its most complex
# function is "init", as it is here that the full FHiCL strings used
# to configure the artdaq processes are configured


class SystemControl
  def initialize(cmdLineOptions, configGen)
    @options = cmdLineOptions
    @configGen = configGen
  end

  def init()
    ebIndex = 0
    agIndex = 0
    totalv1720s = 0
    totalv1724s = 0
    totaltoy1s = 0
    totaltoy2s = 0
    totalasciis = @options.asciis.length
    totalpbrs = @options.pbrs.length
    totaludps = @options.udps.length
    @options.v1720s.each do |proc|
      case proc.kind
      when "V1720"
        totalv1720s += 1
      when "V1724"
        totalv1724s += 1
      end
    end
    @options.toys.each do |proc|
      case proc.kind
      when "TOY1"
        totaltoy1s += 1
      when "TOY2"
        totaltoy2s += 1
      end
    end
    totalBoards = @options.v1720s.length + @options.toys.length + @options.asciis.length + @options.udps.length + @options.pbrs.length
    totalFRs = @options.boardReaders.length
    totalEBs = @options.eventBuilders.length
    totalAGs = @options.aggregators.length
    inputBuffSizeWords = 2097152

    #if Integer(totalv1720s) > 0
    #  inputBuffSizeWords = 8192 * @options.v1720s[0].gate_width
    #end
    xmlrpcClients = @configGen.generateXmlRpcClientList(@options)

    # 02-Dec-2013, KAB - loop over the front-end boards and build the configurations
    # that we will send to them.  These configurations are stored in the associated
    # boardReaders list entries since there are system configurations in which
    # multiple boards are read out by a single BoardReader, and it seems simpler to
    # store the CFGs in the boardReader list for everything

    # John F., 1/21/14 -- added the toy fragment generators

    (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).each { |boardreaderOptions|
      br = @options.boardReaders[boardreaderOptions.board_reader_index]
      listIndex = 0
      br.kindList.each do |kind|
        if kind == boardreaderOptions.kind && br.boardIndexList[listIndex] == boardreaderOptions.index
          if kind == "V1720" || kind == "V1724"
            generatorCode = generateV1720(boardreaderOptions.index,
                                          boardreaderOptions.board_id, kind)
          elsif kind == "pbr"
            if boardreaderOptions.fragType == "V172X"
              generatorCode = generateV1720(boardreaderOptions.index, boardreaderOptions.board_id, "V1720")
            elsif boardreaderOptions.fragType == "ASCII"
              generatorCode = generateAscii(boardreaderOptions.index, boardreaderOptions.board_id, "ASCII", 100000)
            elsif boardreaderOptions.fragType == "TOY1" || boardreaderOptions.fragType == "TOY2"
              case boardreaderOptions.generator_id
              when "Uniform"
                  generatorCode = generateToy(boardreaderOptions.index,
                                          boardreaderOptions.board_id, boardreaderOptions.fragType, 100000, nil, br.eventSize)
              when "Normal"
                  generatorCode = generateNormal(boardreaderOptions.index,
                                          boardreaderOptions.board_id, boardreaderOptions.fragType, 100000, nil, br.eventSize)
              when "Pattern"
                  generatorCode = generatePattern(boardreaderOptions.index,
                                        boardreaderOptions.board_id,  boardreaderOptions.fragType, 100000, nil, br.eventSize)
              else
                 puts "Invalid generator_id!"
                 exit
              end
       
            end
            generatorCode += boardreaderOptions.typeConfig

          elsif kind == "ASCII"
            generatorCode = generateAscii(boardreaderOptions.index, 
                                         boardreaderOptions.board_id, kind, 100000)
          elsif kind == "UDP"
            generatorCode = generateUDP(boardreaderOptions.index, boardreaderOptions.board_id,
                                        boardreaderOptions.udp_port, boardreaderOptions.udp_host )
          elsif kind == "TOY1" || kind == "TOY2"
        
            # The third argument refers to the pause, in us, before
            # generating pseudodata in ToySimulator::getNext_()
            
            case boardreaderOptions.generator_id
            when "Uniform"
                generatorCode = generateToy(boardreaderOptions.index,
                                        boardreaderOptions.board_id, kind, 100000, nil, br.eventSize)
            when "Normal"
                generatorCode = generateNormal(boardreaderOptions.index,
                                        boardreaderOptions.board_id, kind, 100000, nil, br.eventSize)
            when "Pattern"
                generatorCode = generatePattern(boardreaderOptions.index,
                                        boardreaderOptions.board_id, kind, 100000, nil, br.eventSize)
            else
               puts "Invalid generator_id!"
               exit
            end
          end

          cfg = generateBoardReaderMain(totalEBs, totalFRs,
                                        Integer(inputBuffSizeWords/8), 
                                        generatorCode, br.host, br.port)

          br.cfgList[listIndex] = cfg
          break
        end
        listIndex += 1
      end
    }




    threads = []

    (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).each { |proc|
      br = @options.boardReaders[proc.board_reader_index]
      if br.boardCount > 1
        if br.commandHasBeenSent
          next
        else
          br.commandHasBeenSent = true
          proc = br
          cfg = @configGen.generateComposite(totalEBs, totalFRs, br.cfgList)
        end
      else
        cfg = br.cfgList[0]
      end

      currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
      puts "%s: Sending the INIT command to %s:%d." %
        [currentTime, proc.host, proc.port]
      threads << Thread.new() do
        xmlrpcClient = XMLRPC::Client.new(proc.host, "/RPC2",
                                          proc.port)
        if @options.serialize
          fileName = "BoardReader_%s_%s_%d.fcl" % [proc.kind,proc.host, proc.port]
          puts "  writing %s..." % fileName
          handle = File.open(fileName, "w")
          handle.write(cfg)
          handle.close()
        end
        result = xmlrpcClient.call("daq.init", cfg)
        currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
        puts "%s: %s FragmentReceiver on %s:%d result: %s" %
          [currentTime, proc.kind, proc.host, proc.port, result]
        STDOUT.flush
      end
    }
    STDOUT.flush
    threads.each { |aThread|
      aThread.join()
    }


    # 27-Jun-2013, KAB - send INIT to EBs and AG last
    threads = []
    @options.eventBuilders.each { |ebOptions|
      currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
      puts "%s: Sending the INIT command to %s:%d." %
        [currentTime, ebOptions.host, ebOptions.port]
      threads << Thread.new() do
        xmlrpcClient = XMLRPC::Client.new(ebOptions.host, "/RPC2", 
                                          ebOptions.port)

        fclWFViewer = generateWFViewer( (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).map { |board| board.board_id },
                                        (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).map { |board| board.fragType }
                                       )

        puts "HAVE %d v1720s and %d v1724s" % [ totalv1720s, totalv1724s ]

        cfg = generateEventBuilderMain(ebIndex, totalFRs, totalEBs, totalAGs,
                                   ebOptions.compression_level,
                                   totalv1720s, totalv1724s,
                                   @options.dataDir, @options.runOnmon,
                                   @options.writeData, inputBuffSizeWords,
                                   totalBoards, 
                                   fclWFViewer, ebOptions.host, ebOptions.port
                                   )

        if @options.serialize
          fileName = "EventBuilder_%s_%d.fcl" % [ebOptions.host, ebOptions.port]
          puts "  writing %s..." % fileName
          handle = File.open(fileName, "w")
          handle.write(cfg)
          handle.close()
        end
        result = xmlrpcClient.call("daq.init", cfg)
        currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
        puts "%s: EventBuilder on %s:%d result: %s" %
          [currentTime, ebOptions.host, ebOptions.port, result]
        STDOUT.flush
      end
      ebIndex += 1
    }

    @options.aggregators.each { |agOptions|
      currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
      puts "%s: Sending the INIT command to %s:%d" %
        [currentTime, agOptions.host, agOptions.port, agIndex]
      threads << Thread.new( agIndex ) do |agIndexThread|
        xmlrpcClient = XMLRPC::Client.new(agOptions.host, "/RPC2", 
                                          agOptions.port)

        fclWFViewer = generateWFViewer( (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).map { |board| board.board_id },
                                        (@options.v1720s + @options.toys + @options.asciis + @options.udps + @options.pbrs).map { |board| board.fragType }
                                        )

        if @options.onmon_modules = "" || @options.onmon_modules = nil
          @options.onmon_modules = ONMON_MODULES 
        end
        cfg = generateAggregatorMain(@options.dataDir, @options.runNumber,
                                 totalFRs, totalEBs, agOptions.bunch_size,
                                 agOptions.compression_level,
                                 totalv1720s, totalv1724s,
                                 @options.runOnmon, @options.writeData, agOptions.demoPrescale,
                                 agIndexThread, totalAGs, inputBuffSizeWords,
                                 xmlrpcClients, @options.fileSizeThreshold,
                                 @options.fileDurationSeconds,
                                 @options.eventsInFile, fclWFViewer, ONMON_EVENT_PRESCALE,
                                 @options.onmon_modules, @options.onmonFileEnabled, @options.onmonFile,
                                 agOptions.host, agOptions.port)

        if @options.serialize
          fileName = "Aggregator_%s_%d.fcl" % [agOptions.host, agOptions.port]
          puts "  writing %s..." % fileName
          handle = File.open(fileName, "w")
          handle.write(cfg)
          handle.close()
        end
        result = xmlrpcClient.call("daq.init", cfg)
        currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
        puts "%s: Aggregator on %s:%d result: %s" %
          [currentTime, agOptions.host, agOptions.port, result]
        STDOUT.flush
      end

      agIndex += 1
    }
    
    STDOUT.flush
    threads.each { |aThread|
      aThread.join()
    }
  end

  def start(runNumber)
    self.sendCommandSet("start", @options.aggregators, runNumber)
    self.sendCommandSet("start", @options.eventBuilders, runNumber)
    self.sendCommandSet("start", @options.v1720s, runNumber)
    self.sendCommandSet("start", @options.pbrs, runNumber)
    self.sendCommandSet("start", @options.toys, runNumber)
    self.sendCommandSet("start", @options.asciis, runNumber)
    self.sendCommandSet("start", @options.udps, runNumber)
  end

  def sendCommandSet(commandName, procs, commandArg = nil)
    threads = []
    procs.each do |proc|
      # 02-Dec-2013, KAB - use the boardReader instance instead of the
      # actual card when multiple cards are read out by a single BR
      if proc.board_reader_index != nil
        br = @options.boardReaders[proc.board_reader_index]
        if br.boardCount > 1
          if br.commandHasBeenSent
            next
          else
            br.commandHasBeenSent = true
            proc = br
          end
        end
      end

      currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
      puts "%s: Attempting to connect to %s:%d and %s a run." %
        [currentTime, proc.host, proc.port, commandName]
      STDOUT.flush
      threads << Thread.new() do
        xmlrpcClient = XMLRPC::Client.new(proc.host, "/RPC2", proc.port)
        xmlrpcClient.timeout = 60
        if commandName == "stop"
          if proc.kind == "ag"
            xmlrpcClient.timeout = 120
          elsif proc.kind == "eb" || proc.kind == "multi-board"
            xmlrpcClient.timeout = 45
          else
            xmlrpcClient.timeout = 30
          end
        end
        begin
          if commandArg != nil
            result = xmlrpcClient.call("daq.%s" % [commandName], commandArg)
          else
            result = xmlrpcClient.call("daq.%s" % [commandName])
          end
        rescue Exception => msg
          result = "Exception: " + msg
        end
        currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
        case proc.kind
        when "eb"
          puts "%s: EventBuilder on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "ag"
          puts "%s: Aggregator on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "V1720"
          puts "%s: V1720 FragmentReceiver on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "V1724"
          puts "%s: V1724 FragmentReceiver on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "TOY1"
          puts "%s: TOY1 FragmentReceiver on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "TOY2"
          puts "%s: TOY2 FragmentReceiver on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "ASCII"
          puts "%s: ASCII FragmentReceiver on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "UDP"
          puts "%s: UDP FragmentReceiver on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "multi-board"
          puts "%s: multi-board FragmentReceiver on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        when "pbr"
          puts "%s: Preconfigured BoardReader on %s:%d result: %s" %
            [currentTime, proc.host, proc.port, result]
        end
        STDOUT.flush
      end
    end
    threads.each { |aThread|
      aThread.join()
    }
  end

  def shutdown()
    self.sendCommandSet("shutdown", @options.v1720s)
    self.sendCommandSet("shutdown", @options.toys)
    self.sendCommandSet("shutdown", @options.asciis)
    self.sendCommandSet("shutdown", @options.pbrs)
    self.sendCommandSet("shutdown", @options.udps)
    self.sendCommandSet("shutdown", @options.eventBuilders)
    self.sendCommandSet("shutdown", @options.aggregators)
  end

  def pause()
    self.sendCommandSet("pause", @options.v1720s)
    self.sendCommandSet("pause", @options.toys)
    self.sendCommandSet("pause", @options.asciis)
    self.sendCommandSet("pause", @options.pbrs)
    self.sendCommandSet("pause", @options.udps)
    self.sendCommandSet("pause", @options.eventBuilders)
    self.sendCommandSet("pause", @options.aggregators)
  end

  def stop()
    totalAGs = @options.aggregators.length
    if @options.eventsInRun > 0
      if Integer(totalAGs) > 0
        if Integer(totalAGs) > 1
          puts "NOTE: more than one Aggregator is running (count=%d)." % [totalAGs]
          puts " -> The first Aggregator will be used to determine the number of events"
          puts " -> in the current run."
        end
        aggregatorEventCount = 0
        previousAGEventCount = 0
        sleepTime = 0
        while aggregatorEventCount >= 0 && aggregatorEventCount < @options.eventsInRun do
          sleep(sleepTime)
          currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
          puts "%s: Attempting to fetch the number of events from the Aggregator." %
            [currentTime]
          STDOUT.flush
          xmlrpcClient = XMLRPC::Client.new(@options.aggregators[0].host, "/RPC2",
                                            @options.aggregators[0].port)
          xmlrpcClient.timeout = 10
          exceptionOccurred = false
          begin
            result = xmlrpcClient.call("daq.report", "event_count")
            if result == "busy" || result == "-1"
              # support one retry
              sleep(10)
              result = xmlrpcClient.call("daq.report", "event_count")
            end
            aggregatorEventCount = Integer(result)
          rescue Exception => msg
            exceptionOccurred = true
            result = "Exception: " + msg
            aggregatorEventCount = previousAGEventCount
          end
          currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
          if exceptionOccurred
            puts "%s: There was a problem communicating with the Aggregator (%s)," %
              [currentTime, result]
            puts "  the fetch of the number of events will be retried."
          else
            puts "%s: The Aggregator reports the following number of events: %s." %
              [currentTime, result]
          end
          STDOUT.flush

          if aggregatorEventCount > 0 && previousAGEventCount > 0 && \
            aggregatorEventCount > previousAGEventCount && sleepTime > 0 then
            remainingEvents = @options.eventsInRun - aggregatorEventCount
            recentRate = (aggregatorEventCount - previousAGEventCount) / sleepTime
            if recentRate > 0
              sleepTime = (remainingEvents / 2) / recentRate
            else
              sleepTime = 10
            end
            if sleepTime < 10
              sleepTime = 10;
            end
            if sleepTime > 900
              sleepTime = 900;
            end
          else
            sleepTime = 10
          end
          previousAGEventCount = aggregatorEventCount
        end
      else
        puts "No Aggregator in use - Unable to determine the number of events in the current run."     
      end
    elsif @options.runDurationSeconds > 0
      if Integer(totalAGs) > 0
        if Integer(totalAGs) > 1
          puts "NOTE: more than one Aggregator is running (count=%d)." % [totalAGs]
          puts " -> The first Aggregator will be used to determine the run duration."
        end
        aggregatorRunDuration = 0
        sleepTime = 0
        while aggregatorRunDuration >= 0 && aggregatorRunDuration < @options.runDurationSeconds do
          sleep(sleepTime)
          currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
          puts "%s: Attempting to fetch the run duration from the Aggregator." %
            [currentTime]
          STDOUT.flush
          xmlrpcClient = XMLRPC::Client.new(@options.aggregators[0].host, "/RPC2",
                                            @options.aggregators[0].port)
          xmlrpcClient.timeout = 10
          exceptionOccurred = false
          begin
            result = xmlrpcClient.call("daq.report", "run_duration")
            if result == "busy" || result == "-1"
              # support one retry
              sleep(10)
              result = xmlrpcClient.call("daq.report", "run_duration")
            end
            aggregatorRunDuration = Float(result)
          rescue Exception => msg
            exceptionOccurred = true
            result = "Exception: " + msg
            aggregatorRunDuration = 0
          end
          currentTime = DateTime.now.strftime("%Y/%m/%d %H:%M:%S")
          if exceptionOccurred
            puts "%s: There was a problem communicating with the Aggregator (%s)," %
              [currentTime, result]
            puts "  the fetch of the run duration will be retried."
          else
            puts "%s: The Aggregator reports the following run duration: %s seconds." %
              [currentTime, result]
          end
          STDOUT.flush

          if aggregatorRunDuration > 0 then
            remainingTime = @options.runDurationSeconds - aggregatorRunDuration
            sleepTime = remainingTime / 2
            if sleepTime < 10
              sleepTime = 10;
            end
            if sleepTime > 900
              sleepTime = 900;
            end
          else
            sleepTime = 10
          end
        end
      else
        puts "No Aggregator in use - Unable to determine the duration of the current run."
      end
    end

    self.sendCommandSet("stop", @options.v1720s)
    self.sendCommandSet("stop", @options.toys)
    self.sendCommandSet("stop", @options.asciis)
    self.sendCommandSet("stop", @options.pbrs)
    self.sendCommandSet("stop", @options.udps)
    self.sendCommandSet("stop", @options.eventBuilders)
    @options.aggregators.each do |proc|
      tmpList = []
      tmpList << proc
      self.sendCommandSet("stop", tmpList)
    end
  end

  def resume()
    self.sendCommandSet("resume", @options.aggregators)
    self.sendCommandSet("resume", @options.eventBuilders)
    self.sendCommandSet("resume", @options.v1720s)
    self.sendCommandSet("resume", @options.toys)
    self.sendCommandSet("resume", @options.asciis)
    self.sendCommandSet("resume", @options.pbrs)
    self.sendCommandSet("resume", @options.udps)
  end

  def checkStatus()
    self.sendCommandSet("status", @options.aggregators)
    self.sendCommandSet("status", @options.eventBuilders)
    self.sendCommandSet("status", @options.v1720s)
    self.sendCommandSet("status", @options.toys)
    self.sendCommandSet("status", @options.asciis)
    self.sendCommandSet("status", @options.pbrs)
    self.sendCommandSet("status", @options.udps)
  end

  def getLegalCommands()
    self.sendCommandSet("legal_commands", @options.aggregators)
    self.sendCommandSet("legal_commands", @options.eventBuilders)
    self.sendCommandSet("legal_commands", @options.v1720s)
    self.sendCommandSet("legal_commands", @options.toys)
    self.sendCommandSet("legal_commands", @options.asciis)
    self.sendCommandSet("legal_commands", @options.pbrs)
    self.sendCommandSet("legal_commands", @options.udps)
  end
end

if __FILE__ == $0

  # Create an instance of the class charged with generating FHiCL configuration scripts
  cfgGen = ConfigGen.new

  # And pass it, to be filled, to an instance of the class used to
  # parse the arguments passed to the command line

  cmdLineParser = CommandLineParser.new(cfgGen)
  cmdLineParser.parse()

  # Obtain the structure containing the command line options
  options = cmdLineParser.getOptions()
  puts "DemoControl disk writing setting = " + options.writeData

  if options.summary
    cmdLineParser.summarize()
  end


  # Create an instance of the class used to implement the transition
  # command passed to this script as an argument

  sysCtrl = SystemControl.new(options, cfgGen)

  if options.command == "init"
    sysCtrl.init()
  elsif options.command == "start"
    sysCtrl.start(options.runNumber)
  elsif options.command == "stop"
    sysCtrl.stop()
  elsif options.command == "shutdown"
    sysCtrl.shutdown()
  elsif options.command == "pause"
    sysCtrl.pause()
  elsif options.command == "resume"
    sysCtrl.resume()
  elsif options.command == "status"
    sysCtrl.checkStatus()
  elsif options.command == "get-legal-commands"
    sysCtrl.getLegalCommands()
  end
end
