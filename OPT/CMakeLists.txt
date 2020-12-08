set (CMAKE_CXX_STANDARD 17)
add_llvm_library(OPT MODULE            # Name of the shared library that you will generate
  optpass.cpp                          # Your pass
  PLUGIN_TOOL
  opt
)
