#!/bin/bash
### run.sh
### benchmark runner script
### Locate this script at each benchmark directory. e.g, 583simple/run.sh
### usage: ./run.sh ${benchmark_name} ${input}
### e.g., ./run.sh compress compress.in or ./run.sh simple

PATH_MYPASS=~/eecs583fp/build/OPTIM/OPTIM.so ### Action Required: Specify the path to your pass ###
NAME_MYPASS=-fp_optim ### Action Required: Specify the name for your pass ###
BENCH_NAME=${1}
INPUT=${2}
BENCH=src/${BENCH_NAME}.c


./run_profile.sh ${BENCH_NAME} ${INPUT}

clang -emit-llvm -c ${BENCH} -o ${BENCH_NAME}.bc 
opt -load ${PATH_MYPASS} ${NAME_MYPASS} < ${BENCH_NAME}.bc > ${BENCH_NAME}.opt.bc

clang -lm ${BENCH_NAME}.opt.bc && ./a.out ${INPUT} # need -lm for sqrt()
