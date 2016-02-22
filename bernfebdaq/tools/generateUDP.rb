
# Generate the FHiCL document which configures the demo::UDPReceiver class

require File.join( File.dirname(__FILE__), 'demo_utilities' )
  
def generateUDP(startingFragmentId, boardId, 
                port, address, rawOutputEnabled = false, rawOutputPath = "/tmp")

  udpConfig = String.new( "\
    generator: UDPReceiver
    fragment_type: UDP
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    trigger_port: 5001
    trigger_mode: \"Untriggered\"
    sleep_on_stop_us: 500000 " \
                          + read_fcl("UDPReceiver.fcl") )
  
  udpConfig.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  udpConfig.gsub!(/\%\{board_id\}/, String(boardId))
  udpConfig.gsub!(/\%\{fragment_type\}/, String("UDP")) 
  udpConfig.gsub!(/\%\{port\}/, String(port))
  udpConfig.gsub!(/\%\{address\}/,String(address))

  return udpConfig

end
