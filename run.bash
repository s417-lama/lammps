#!/bin/bash
set -euo pipefail
export LC_ALL=C
export LANG=C

cd -P $(dirname $0)/src

# export ABT_SET_AFFINITY=0
# # # mpirun ./lmp_omp -in ../bench/in.lj -var x 4 -var y 6 -var z 8 -sf omp
# # numactl -iall mpirun ./lmp_omp -in ../bench/in.lj -var x 4 -var y 6 -var z 8 -sf omp
# # numactl -iall mpirun ./lmp_omp -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -sf omp
# mpirun ./lmp_omp -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -sf omp

export LAMMPS_ABT_NUM_XSTREAMS=8
export LAMMPS_ABT_NUM_THREADS=8
# mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 8 -sf omp
# numactl -iall mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 8 -sf omp
# numactl -iall mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -sf omp
# mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -sf omp
mpirun ./lmp_mpi -in ../bench/in.lj -var x 8 -var y 8 -var z 8 -sf omp
