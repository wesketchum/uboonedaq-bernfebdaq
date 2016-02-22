
# Generate the FHiCL document which configures the demo::AsciiSimulator class

require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateAscii(startingFragmentId, boardId, 
                fragmentType, throttleUsecs = nil)

  asciiConfig = String.new( "\
    generator: AsciiSimulator
    fragment_type: %{fragment_type}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    sleep_on_stop_us: 500000 " \
                          + read_fcl("AsciiSimulator.fcl") )
  
  asciiConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  asciiConfig.gsub!(/\%\{board_id\}/, String(boardId))
  asciiConfig.gsub!(/\%\{fragment_type\}/, String(fragmentType)) 

  if ! throttleUsecs.nil?
    asciiConfig.gsub!(/.*throttle_usecs.*\:.*/, "throttle_usecs: %d" % [throttleUsecs])
  end

  return asciiConfig

end
