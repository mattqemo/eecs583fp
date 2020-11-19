add_llvm_library(LLVMHW1 MODULE            # Name of the shared library that you will generate
  mypass.cpp                          # Your pass
  PLUGIN_TOOL
  opt
)