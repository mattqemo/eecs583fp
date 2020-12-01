add_llvm_library(PROFILE MODULE            # Name of the shared library that you will generate
  profilepass.cpp                          # Your pass
  PLUGIN_TOOL
  opt
)
