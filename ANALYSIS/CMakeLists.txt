set (CMAKE_CXX_STANDARD 17)
add_llvm_library(ANALYSIS MODULE            # Name of the shared library that you will generate
  analysispass.cpp                          # Your pass
  PLUGIN_TOOL
  opt
)
