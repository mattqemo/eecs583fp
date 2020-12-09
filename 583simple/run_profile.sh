#!/bin/bash
### run.sh
### benchmark runner script
### Locate this script at each benchmark directory. e.g, 583simple/run.sh
### usage: ./run.sh ${benchmark_name} ${input}
### e.g., ./run.sh compress compress.in or ./run.sh simple

PATH_MYPASS=~/eecs583fp/build/PROFILE/PROFILE.so ### Action Required: Specify the path to your pass ###
NAME_MYPASS=-fp_profile ### Action Required: Specify the name for your pass ###
BENCH=src/simple.c
INPUT=${2}

clang -emit-llvm -c ${BENCH} -o simple.bc
opt -load ${PATH_MYPASS} ${NAME_MYPASS} < simple.bc > simple.prof.bc

clang simple.prof.bc && ./a.out
