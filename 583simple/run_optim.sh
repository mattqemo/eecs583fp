#!/bin/bash
### run.sh
### benchmark runner script
### Locate this script at each benchmark directory. e.g, 583simple/run.sh
### usage: ./run.sh ${benchmark_name} ${input}
### e.g., ./run.sh compress compress.in or ./run.sh simple

PATH_MYPASS=~/eecs583fp/build/OPTIM/OPTIM.so ### Action Required: Specify the path to your pass ###
NAME_MYPASS=-fp_funcoptim
BENCH_NAME=${1}
INPUT_PROFILE=${2}
INPUT_ACTUAL=${3}
BENCH=src/${BENCH_NAME}.c

# echo "RUNOPT: profiling..."
./run_profile.sh ${BENCH_NAME} ${INPUT_PROFILE} > /dev/null
# echo "RUNOPT: building for optimization..."

# echo "RUNOPT: timing compilation of unoptimized code..."
# time -p sh -c "clang -emit-llvm -c ${BENCH} -o ${BENCH_NAME}.bc &&\
#                clang -lm ${BENCH_NAME}.bc"
clang -emit-llvm -c ${BENCH} -o ${BENCH_NAME}.bc

clang -lm ${BENCH_NAME}.bc

# echo "RUNOPT: timing unoptimized code runtime..."
# time ./a.out "${INPUT_ACTUAL}" > /dev/null
time./a.out "${INPUT_ACTUAL}" > /dev/null

# echo "RUNOPT: timing compilation of OPTIMIZED code..."
# time -p sh -c "clang -emit-llvm -c ${BENCH} -o ${BENCH_NAME}.bc &&\
#                 opt -load ${PATH_MYPASS} ${NAME_MYPASS} < ${BENCH_NAME}.bc > ${BENCH_NAME}.opt.bc &&\
#                 clang -lm ${BENCH_NAME}.opt.bc" # need -lm for sqrt()

opt -load ${PATH_MYPASS} ${NAME_MYPASS} < ${BENCH_NAME}.bc > ${BENCH_NAME}.opt.bc
clang -lm ${BENCH_NAME}.opt.bc # need -lm for sqrt()
# echo "RUNOPT: timing OPTIMIZED code runtime..."

# capture output of time and classex by redirecting stderr
# run grep to only collect real runtime and parameter (percent likelihood)
likelihoods=(100 90 80 70 60 50 40 30 20 10 0)
for l in ${likelihoods[@]}; do
    INPUT="2,2,100000,${l},1"
    (time ./a.out "${INPUT}") 2>&1 > /dev/null |grep -E -- 'percent|real' >> classex_stats.txt
done