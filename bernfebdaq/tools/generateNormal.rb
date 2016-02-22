
# Generate the FHiCL document which configures the demo::NormalSimulator class

# Note that if "nADCcounts" is set to nil, its FHiCL
# setting is defined in a separate file called NormalSimulator.fcl,
# searched for via "read_fcl"

require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateNormal(startingFragmentId, boardId, 
                fragmentType, throttleUsecs = nil, nADCcounts = nil, eventSize = nil)

  normalConfig = String.new( "\
    generator: NormalSimulator
    fragment_type: %{fragment_type}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    random_seed: %{random_seed}
    sleep_on_stop_us: 500000 " \
                          + read_fcl("NormalSimulator.fcl") )
  
  normalConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  normalConfig.gsub!(/\%\{board_id\}/, String(boardId))
  normalConfig.gsub!(/\%\{random_seed\}/, String(rand(10000))) 
  normalConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 

  if ! nADCcounts.nil?
    normalConfig.gsub!(/.*nADCcounts.*\:.*/, "nADCcounts: %d" % [nADCcounts])
  end

  if ! eventSize.nil?
    adcCount = eventSize / 2
    normalConfig.gsub!(/.*nADCcounts.*\:.*/, "nADCcounts: %d" % [adcCounts])
  end

  if ! throttleUsecs.nil?
    normalConfig.gsub!(/.*throttle_usecs.*\:.*/, "throttle_usecs: %d" % [throttleUsecs])
  end

  return normalConfig

end
