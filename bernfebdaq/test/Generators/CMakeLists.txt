cet_test(V172xFileReader_t HANDBUILT
  TEST_EXEC genToArt
  TEST_ARGS -c V172xFileReader_t.fcl
  DATAFILES
  fcl/V172xFileReader_t.fcl
  V1720_sample.dat
)

cet_test(V172xSimulator_t HANDBUILT
  TEST_EXEC genToArt
  TEST_ARGS -c V172xSimulator_t.fcl
  DATAFILES
  fcl/V172xSimulator_t.fcl
  V1720_sample_freqs.dat
)

cet_test(ToySimulator_t HANDBUILT
  TEST_EXEC genToArt
  TEST_ARGS -c ToySimulator_t.fcl
  DATAFILES
  fcl/ToySimulator_t.fcl
)
