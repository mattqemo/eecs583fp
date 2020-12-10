set (CMAKE_CXX_STANDARD 17)
add_llvm_library(OPTIM MODULE            # Name of the shared library that you will generate
  optimpass.cpp                          # Your pass
  PLUGIN_TOOL
  opt
)
