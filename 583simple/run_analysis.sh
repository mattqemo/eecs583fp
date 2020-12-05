#!/bin/bash
### run.sh
### benchmark runner script
### Locate this script at each benchmark directory. e.g, 583simple/run.sh
### usage: ./run.sh ${benchmark_name} ${input}
### e.g., ./run.sh compress compress.in or ./run.sh simple

PATH_MYPASS=~/eecs583fp/build/ANALYSIS/ANALYSIS.so ### Action Required: Specify the path to your pass ###
NAME_MYPASS=-fp_analysis ### Action Required: Specify the name for your pass ###
BENCH=src/simple.c


# # Prepare input to run
# setup
# # Convert source code to bitcode (IR)
# # This approach has an issue with -O2, so we are going to stick with default optimization level (-O0)
clang -emit-llvm -c ${BENCH} -o simple.bc
# # Instrument profiler
# opt -pgo-instr-gen -instrprof simple.bc -o simple.prof.bc
# # Generate binary executable with profiler embedded
# clang -fprofile-instr-generate simple.prof.bc -o simple.prof
# # Collect profiling data
# ./simple.prof ${INPUT} > /dev/null 2>&1
# # Translate raw profiling data into LLVM data format
# llvm-profdata merge -output=pgo.profdata default.profraw

# Prepare input to run
# setup
# Apply your pass to bitcode (IR)
opt -load ${PATH_MYPASS} ${NAME_MYPASS} < simple.bc > simple.opt.bc

clang simple.opt.bc && ./a.out
