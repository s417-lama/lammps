#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

export HFI_NO_CPUAFFINITY=yes
# export ABT_SET_AFFINITY=0
export LAMMPS_ABT_NUM_XSTREAMS=56
export LAMMPS_ABT_NUM_THREADS=56

N_REPEATS=10
ANALYSIS_INTVLS=(1 2)
ANALYSIS_THREADS=(55 110 220 440 880 1760 3520 7040 14080)

DATETIME=${DATETIME:-$(TZ='America/Chicago' date +%Y-%m-%d_%H-%M-%S)}
RESULT_DIR=${RESULT_DIR:-${PWD}/results}
JOB_DIR=${RESULT_DIR}/${DATETIME}

mkdir -p $JOB_DIR

cd src

run_lammps() {
  local NAME=$1
  for intvl in ${ANALYSIS_INTVLS[@]}; do
    for thread in ${ANALYSIS_THREADS[@]}; do
      for i in $(seq 1 $N_REPEATS); do
        export LAMMPS_ANALYSIS_INTERVAL=$intvl
        export LAMMPS_NUM_ANALYSIS_THREADS=$thread

        local OUT_FILE=${JOB_DIR}/${NAME}_intvl_${intvl}_thread_${thread}_${i}.out
        local ERR_FILE=${JOB_DIR}/${NAME}_intvl_${intvl}_thread_${thread}_${i}.err

        numactl -iall mpirun ./lmp_kokkos_abt -in ../bench/in.lj -var x 8 -var y 8 -var z 8 -k on t 56 -sf kk -pk kokkos newton off neigh full 2> $ERR_FILE | tee $OUT_FILE
      done
    done
  done
}

export LAMMPS_ENABLE_ANALYSIS=0
export LAMMPS_ASYNC_ANALYSIS=0
export LAMMPS_ABT_ENABLE_PREEMPTION=0
run_lammps no_analysis

export LAMMPS_ENABLE_ANALYSIS=1
export LAMMPS_ASYNC_ANALYSIS=0
export LAMMPS_ABT_ENABLE_PREEMPTION=0
run_lammps sync

export LAMMPS_ENABLE_ANALYSIS=1
export LAMMPS_ASYNC_ANALYSIS=1
export LAMMPS_ABT_ENABLE_PREEMPTION=0
run_lammps async

export LAMMPS_ENABLE_ANALYSIS=1
export LAMMPS_ASYNC_ANALYSIS=1
export LAMMPS_ABT_ENABLE_PREEMPTION=1
run_lammps preemption

ln -sfn ${JOB_DIR} ${RESULT_DIR}/latest
