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

# export LAMMPS_ABT_NUM_XSTREAMS=56
# export LAMMPS_ABT_NUM_THREADS=56
# # mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 8 -sf omp
# # numactl -iall mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 8 -sf omp
# # numactl -iall mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -sf omp
# mpirun ./lmp_mpi -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -sf omp
# # mpirun ./lmp_mpi -in ../bench/in.lj -var x 8 -var y 8 -var z 8 -sf omp

export HFI_NO_CPUAFFINITY=yes
# export ABT_SET_AFFINITY=0
export LAMMPS_ABT_NUM_XSTREAMS=56
export LAMMPS_ABT_NUM_THREADS=56
# mpirun ./lmp_kokkos_omp -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -k on t 56 -sf kk
# numactl -iall mpirun ./lmp_kokkos_omp -in ../bench/in.lj -var x 4 -var y 6 -var z 4 -k on t 56 -sf kk -pk kokkos newton off neigh full
numactl -iall mpirun ./lmp_kokkos_abt -in ../bench/in.lj -var x 8 -var y 8 -var z 8 -k on t 56 -sf kk -pk kokkos newton off neigh full
