
# Generate the FHiCL document which configures the demo::ToySimulator class

# Note that if "nADCcounts" is set to nil, its FHiCL
# setting is defined in a separate file called ToySimulator.fcl,
# searched for via "read_fcl"

require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generatePattern(startingFragmentId, boardId, 
                fragmentType, throttleUsecs = nil, nADCcounts = nil, eventSize = nil)

  patternConfig = String.new( "\
    generator: PatternSimulator
    fragment_type: %{fragment_type}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    random_seed: %{random_seed}
    sleep_on_stop_us: 500000 " \
                          + read_fcl("PatternSimulator.fcl") )
  
  patternConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  patternConfig.gsub!(/\%\{board_id\}/, String(boardId))
  patternConfig.gsub!(/\%\{random_seed\}/, String(rand(10000))) 
  patternConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 

  if ! nADCcounts.nil?
    patternConfig.gsub!(/.*nADCcounts.*\:.*/, "nADCcounts: %d" % [nADCcounts])
  end

  if ! eventSize.nil?
    adcCounts = eventSize / 2
    patternConfig.gsub!(/.*nADCcounts.*\:.*/, "nADCcounts: %d" % [adcCounts])
  end

  if ! throttleUsecs.nil?
    patternConfig.gsub!(/.*throttle_usecs.*\:.*/, "throttle_usecs: %d" % [throttleUsecs])
  end

  return patternConfig

end
