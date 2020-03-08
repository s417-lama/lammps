/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "lammps.h"
#include <mpi.h>
#include "input.h"

#if defined(LAMMPS_TRAP_FPE) && defined(_GNU_SOURCE)
#include <fenv.h>
#endif

#ifdef FFT_FFTW3
#include <fftw3.h>
#endif

#if defined(LAMMPS_EXCEPTIONS)
#include "exceptions.h"
#endif

#if defined(_OPENMP)
#include <omp.h>
#endif

#include "logger.h"
#include "threads.h"

using namespace LAMMPS_NS;

/* ----------------------------------------------------------------------
   main program to drive LAMMPS
------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
  MPI_Init(&argc,&argv);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  char *s;
  s = getenv("LAMMPS_ABT_NUM_XSTREAMS");
  if (s) {
    g_num_xstreams = atoi(s);
  } else {
    g_num_xstreams = 1;
  }

  s = getenv("LAMMPS_ABT_NUM_THREADS");
  if (s) {
    g_num_threads = atoi(s);
  } else {
    g_num_threads = 1;
  }

  int n_logger;
#if defined(_OPENMP)
#pragma omp parallel default(none) shared(n_logger)
  n_logger = omp_get_num_threads();
#else
  n_logger = g_num_xstreams;
#endif
  logger_init(n_logger);

  int ret;
  ABT_init(0, NULL);

  /* Create pools */
  g_pools = (ABT_pool *)malloc(sizeof(ABT_pool) * g_num_xstreams);
  for (int i = 0; i < g_num_xstreams; i++) {
    ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &g_pools[i]);
  }

  /* Primary ES creation */
  ABT_xstream *xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * g_num_xstreams);
  ABT_xstream_self(&xstreams[0]);
  ABT_sched sched;
  ABT_sched_create_basic(SCHED_TYPE, 1, &g_pools[0], ABT_SCHED_CONFIG_NULL, &sched);
  ABT_xstream_set_main_sched(xstreams[0], sched);

  /* Secondary ES creation */
  for (int i = 1; i < g_num_xstreams; i++) {
    ret = ABT_xstream_create_basic(SCHED_TYPE, 1, &g_pools[i], ABT_SCHED_CONFIG_NULL, &xstreams[i]);
    HANDLE_ERROR(ret, "ABT_xstream_create");
  }

  /* Barrier */
  ABT_barrier_create(g_num_threads, &g_barrier);

  /* Mutex */
  ABT_mutex_create(&g_mutex);

#if ENABLE_PREEMPTION
  ABT_preemption_group *preemption_groups = (ABT_preemption_group *)malloc(sizeof(ABT_preemption_group) * num_pgroups);
  ABT_preemption_timer_create_groups(num_pgroups, preemption_groups);
  for (int i = 0; i < num_pgroups; i++) {
    int begin = i * g_num_xstreams / num_pgroups;
    int end   = (i + 1) * g_num_xstreams / num_pgroups;
    if (end > g_num_xstreams) end = g_num_xstreams;
    ABT_preemption_timer_set_xstreams(preemption_groups[i], end - begin, &xstreams[begin]);
  }
#endif

  // enable trapping selected floating point exceptions.
  // this uses GNU extensions and is only tested on Linux
  // therefore we make it depend on -D_GNU_SOURCE, too.

#if defined(LAMMPS_TRAP_FPE) && defined(_GNU_SOURCE)
  fesetenv(FE_NOMASK_ENV);
  fedisableexcept(FE_ALL_EXCEPT);
  feenableexcept(FE_DIVBYZERO);
  feenableexcept(FE_INVALID);
  feenableexcept(FE_OVERFLOW);
#endif

#ifdef LAMMPS_EXCEPTIONS
  try {
    LAMMPS *lammps = new LAMMPS(argc,argv,MPI_COMM_WORLD);
    lammps->input->file();
    delete lammps;
  } catch(LAMMPSAbortException & ae) {
    MPI_Abort(ae.universe, 1);
  } catch(LAMMPSException & e) {
    MPI_Finalize();
    exit(1);
  }
#else
  LAMMPS *lammps = new LAMMPS(argc,argv,MPI_COMM_WORLD);
  lammps->input->file();
  delete lammps;
#endif

#if ENABLE_PREEMPTION
  for (int i = 0; i < num_pgroups; i++) {
    ABT_preemption_timer_delete(preemption_groups[i]);
  }
  free(preemption_groups);
#endif

  /* Join and free Execution Streams */
  for (int i = 1; i < g_num_xstreams; i++) {
    ret = ABT_xstream_free(&xstreams[i]);
    HANDLE_ERROR(ret, "ABT_xstream_free");
  }

  /* Free barrier */
  ABT_barrier_free(&g_barrier);

  /* Free mutex */
  ABT_mutex_free(&g_mutex);

  ret = ABT_finalize();
  HANDLE_ERROR(ret, "ABT_finalize");

  free(xstreams);
  free(g_pools);

  char filename[20];
  sprintf(filename, "mlog_%d.ignore", rank);
  FILE *fp = fopen(filename, "w+");
  logger_flush(fp);
  fclose(fp);

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

#ifdef FFT_FFTW3
  // tell fftw3 to delete its global memory pool
  // and thus avoid bogus valgrind memory leak reports
#ifdef FFT_SINGLE
  fftwf_cleanup();
#else
  fftw_cleanup();
#endif
#endif
}
