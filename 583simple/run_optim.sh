#!/bin/bash
### run.sh
### benchmark runner script
### Locate this script at each benchmark directory. e.g, 583simple/run.sh
### usage: ./run.sh ${benchmark_name} ${input}
### e.g., ./run.sh compress compress.in or ./run.sh simple

PATH_MYPASS=~/eecs583fp/build/OPTIM/OPTIM.so ### Action Required: Specify the path to your pass ###
NAME_MYPASS=-fp_optim ### Action Required: Specify the name for your pass ###
BENCH_NAME=${1}
INPUT_PROFILE=${2}
INPUT_ACTUAL=${3}
BENCH=src/${BENCH_NAME}.c

echo "RUNOPT: profiling..."
./run_profile.sh ${BENCH_NAME} ${INPUT_PROFILE} > /dev/null
echo "RUNOPT: building for optimization..."

echo "RUNOPT: timing compilation of unoptimized code..."
time -p sh -c "clang -emit-llvm -c ${BENCH} -o ${BENCH_NAME}.bc &&\
               clang -lm ${BENCH_NAME}.bc"
echo "RUNOPT: timing unoptimized code runtime..."
time ./a.out "${INPUT_ACTUAL}" > /dev/null

echo "RUNOPT: timing compilation of OPTIMIZED code..."
time -p sh -c "clang -emit-llvm -c ${BENCH} -o ${BENCH_NAME}.bc &&\
                opt -load ${PATH_MYPASS} ${NAME_MYPASS} < ${BENCH_NAME}.bc > ${BENCH_NAME}.opt.bc &&\
                clang -lm ${BENCH_NAME}.opt.bc" # need -lm for sqrt()
echo "RUNOPT: timing OPTIMIZED code runtime..."
time ./a.out "${INPUT_ACTUAL}" > /dev/null
