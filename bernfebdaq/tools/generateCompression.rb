

def generateCompression( instance_name )

compressionConfig = String.new( "\
module_type: Compression
raw_label: daq
instance_name: %{instance_name}
table_file: \"table_daq%{instance_name}_huff_diff.txt\"
perf_print: false
use_diffs: true
record_compression: false
"
)

compressionConfig.gsub!(/\%\{instance_name\}/, String(instance_name))

return compressionConfig

end



def generateDecompression( instance_name )

decompressionConfig = String.new( "\
module_type: Decompression
instance_name: %{instance_name}
compressed_label: huffdiff%{instance_name}
table_file:  \"table_daq%{instance_name}_huff_diff.txt\"
")

decompressionConfig.gsub!(/\%\{instance_name\}/, String(instance_name))

return decompressionConfig

end
