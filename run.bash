#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/

cd src

export HFI_NO_CPUAFFINITY=yes
# export ABT_SET_AFFINITY=0
export LAMMPS_ABT_NUM_XSTREAMS=56
export LAMMPS_ABT_NUM_THREADS=56

export LAMMPS_ANALYSIS_INTERVAL=1
export LAMMPS_NUM_ANALYSIS_THREADS=440

run_lammps_omp() {
  export OMP_PROC_BIND=true
  export KMP_BLOCKTIME=0
  numactl -iall mpirun ./lmp_kokkos_omp -in ../bench/in.lj -var x 8 -var y 8 -var z 8 -k on t 56 -sf kk -pk kokkos newton off neigh full
}

run_lammps_abt() {
  numactl -iall mpirun ./lmp_kokkos_abt -in ../bench/in.lj -var x 8 -var y 8 -var z 8 -k on t 56 -sf kk -pk kokkos newton off neigh full
}

# export LAMMPS_ENABLE_ANALYSIS=0
# export LAMMPS_ASYNC_ANALYSIS=0
# run_lammps_omp

# export LAMMPS_ENABLE_ANALYSIS=1
# export LAMMPS_ASYNC_ANALYSIS=1
# run_lammps_omp

export LAMMPS_ENABLE_ANALYSIS=0
export LAMMPS_ASYNC_ANALYSIS=0
export LAMMPS_ABT_ENABLE_PREEMPTION=0
run_lammps_abt

export LAMMPS_ENABLE_ANALYSIS=1
export LAMMPS_ASYNC_ANALYSIS=0
export LAMMPS_ABT_ENABLE_PREEMPTION=0
run_lammps_abt

export LAMMPS_ENABLE_ANALYSIS=1
export LAMMPS_ASYNC_ANALYSIS=1
export LAMMPS_ABT_ENABLE_PREEMPTION=0
run_lammps_abt

export LAMMPS_ENABLE_ANALYSIS=1
export LAMMPS_ASYNC_ANALYSIS=1
export LAMMPS_ABT_ENABLE_PREEMPTION=1
run_lammps_abt
