

# Generate the FHiCL document which configures the demo::V172xSimulator class

# Note that if "nChannels" is set to nil, it is defined in
# a separate file called V172xSimulator.fcl, searched for via "read_fcl"


require File.join( File.dirname(__FILE__), 'demo_utilities' )

def generateV1720(startingFragmentId, boardId, 
                  fragmentType, nChannels = nil)

v1720Config = String.new( "\

    generator: V172xSimulator
    fragment_type: %{fragment_type}
    freqs_file: \"V1720_sample_freqs.dat\"
    starting_fragment_id: %{starting_fragment_id}
    fragment_id: %{starting_fragment_id}
    board_id: %{board_id}
    random_seed: %{random_seed}
    sleep_on_stop_us: 500000 ")

  v1720Config.gsub!(/\%\{starting_fragment_id\}/, String(startingFragmentId))
  v1720Config.gsub!(/\%\{board_id\}/, String(boardId))
  v1720Config.gsub!(/\%\{random_seed\}/, String(rand(10000))) 
  v1720Config.gsub!(/\%\{fragment_type\}/, fragmentType) 


  if ! nChannels.nil?
    v1720Config += "\nnChannels: %d\n" % [ nChannels ]
  else
    v1720Config +=  read_fcl("V172xSimulator.fcl")
  end

  return v1720Config

end
