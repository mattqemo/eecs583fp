set (CMAKE_CXX_STANDARD 17)
cmake_minimum_required(VERSION 3.4.3)
find_package(LLVM REQUIRED CONFIG)                        # This will find the shared LLVM build.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")        # You don't need to change ${LLVM_CMAKE_DIR} since it is already defined.
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})                      # You don't need to change ${LLVM_DEFINITIONS} since it is already defined.
include_directories(${LLVM_INCLUDE_DIRS})                 # You don't need to change ${LLVM_INCLUDE_DIRS} since it is already defined.
add_subdirectory(PROFILE)                                  # Add the directory which your pass lives.
add_subdirectory(ANALYSIS)                                  # Add the directory which your pass lives.
