/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#include <Kokkos_Macros.hpp>
#if defined( KOKKOS_ENABLE_ARGOBOTS )

#include <abt.h>
#include <mpi.h>

#include <cstdio>
#include <cstdlib>

#include <limits>
#include <iostream>
#include <vector>

#include <Kokkos_Core.hpp>

#include <impl/Kokkos_Error.hpp>
#include <impl/Kokkos_CPUDiscovery.hpp>
#include <impl/Kokkos_Profiling_Interface.hpp>

#include "logger.h"

namespace Kokkos {
namespace Impl {

ABT_pool* g_pools;
ABT_pool g_analysis_pool;
int g_num_xstreams;
int g_num_threads;
int g_num_pgroups;
ABT_barrier g_barrier;
ABT_mutex g_mutex;
ABT_xstream* g_xstreams;
ABT_sched* g_scheds;

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
  return ABT_SUCCESS;
}

static void sched_run(ABT_sched sched)
{
  uint32_t work_count = 0;
  int num_pools;
  ABT_pool *pools;
  ABT_unit unit;
  ABT_bool stop;
  int event_freq = 100;

  ABT_sched_get_num_pools(sched, &num_pools);
  pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
  ABT_sched_get_pools(sched, num_pools, 0, pools);

  int rank;
  ABT_xstream_self_rank(&rank);

  while (1) {
    ABT_pool_pop(pools[0], &unit);
    if (unit != ABT_UNIT_NULL) {
      ABT_xstream_run_unit(unit, pools[0]);
    } else {
      /* ABT_pool_pop(pools[1], &unit); */
      /* if (unit != ABT_UNIT_NULL) { */
      /*   void *bp = logger_begin_tl(rank); */

      /*   ABT_xstream_run_unit(unit, pools[1]); */

      /*   logger_end_tl(rank, bp, "analysis"); */
      /* } */
    }

    if (++work_count >= event_freq) {
      work_count = 0;
      ABT_sched_has_to_stop(sched, &stop);
      if (stop == ABT_TRUE) break;
      ABT_xstream_check_events(sched);
    }
  }

  free(pools);
}

static int sched_free(ABT_sched sched)
{
  return ABT_SUCCESS;
}

static ABT_sched_def sched_def = {
  .type = ABT_SCHED_TYPE_ULT,
  .init = sched_init,
  .run = sched_run,
  .free = sched_free,
  .get_migr_pool = NULL
};

} // namespace Impl
} // namespace Kokkos

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

namespace Kokkos {

//----------------------------------------------------------------------------

#ifdef KOKKOS_ENABLE_DEPRECATED_CODE
void Argobots::initialize()
#else
void Argobots::impl_initialize()
#endif
{
  char *s;
  s = getenv("LAMMPS_ABT_NUM_XSTREAMS");
  if (s) {
    Impl::g_num_xstreams = atoi(s);
  } else {
    Impl::g_num_xstreams = 1;
  }

  s = getenv("LAMMPS_ABT_NUM_THREADS");
  if (s) {
    Impl::g_num_threads = atoi(s);
  } else {
    Impl::g_num_threads = 1;
  }

  logger_init(Impl::g_num_xstreams);

  int ret;
  ABT_init(0, NULL);

  /* Create pools */
  Impl::g_pools = (ABT_pool *)malloc(sizeof(ABT_pool) * Impl::g_num_xstreams);
  for (int i = 0; i < Impl::g_num_xstreams; i++) {
    ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &Impl::g_pools[i]);
  }
  ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &Impl::g_analysis_pool);

  /* Create scheds */
  Impl::g_scheds = (ABT_sched *)malloc(sizeof(ABT_sched) * Impl::g_num_xstreams);
  for (int i = 0; i < Impl::g_num_xstreams; i++) {
    ABT_pool mypools[2];
    mypools[0] = Impl::g_pools[i];
    mypools[1] = Impl::g_analysis_pool;
    ABT_sched_create(&Impl::sched_def, 2, mypools, ABT_SCHED_CONFIG_NULL, &Impl::g_scheds[i]);
  }

  /* Primary ES creation */
  Impl::g_xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * Impl::g_num_xstreams);
  ABT_xstream_self(&Impl::g_xstreams[0]);
  ABT_xstream_set_main_sched(Impl::g_xstreams[0], Impl::g_scheds[0]);

  /* Secondary ES creation */
  for (int i = 1; i < Impl::g_num_xstreams; i++) {
    ret = ABT_xstream_create(Impl::g_scheds[i], &Impl::g_xstreams[i]);
    HANDLE_ERROR(ret, "ABT_xstream_create");
  }

  /* Barrier */
  ABT_barrier_create(Impl::g_num_threads, &Impl::g_barrier);

  /* Mutex */
  ABT_mutex_create(&Impl::g_mutex);

  Impl::g_num_pgroups = 56;
#if ENABLE_PREEMPTION
  ABT_preemption_group *preemption_groups = (ABT_preemption_group *)malloc(sizeof(ABT_preemption_group) * Impl::g_num_pgroups);
  ABT_preemption_timer_create_groups(Impl::g_num_pgroups, preemption_groups);
  for (int i = 0; i < Impl::g_num_pgroups; i++) {
    int begin = i * Impl::g_num_xstreams / Impl::g_num_pgroups;
    int end   = (i + 1) * Impl::g_num_xstreams / Impl::g_num_pgroups;
    if (end > Impl::g_num_xstreams) end = Impl::g_num_xstreams;
    ABT_preemption_timer_set_xstreams(preemption_groups[i], end - begin, &Impl::g_xstreams[begin]);
  }
  for (int i = 0; i < Impl::g_num_pgroups; i++) {
    ABT_preemption_timer_start(preemption_groups[i]);
  }
#endif
}

//----------------------------------------------------------------------------

#ifdef KOKKOS_ENABLE_DEPRECATED_CODE
void Argobots::finalize()
#else
void Argobots::impl_finalize()
#endif
{
  int ret;

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

#if ENABLE_PREEMPTION
  for (int i = 0; i < Impl::g_num_pgroups; i++) {
    ABT_preemption_timer_delete(preemption_groups[i]);
  }
  free(preemption_groups);
#endif

  /* Join and free Execution Streams */
  for (int i = 1; i < Impl::g_num_xstreams; i++) {
    ret = ABT_xstream_free(&Impl::g_xstreams[i]);
    HANDLE_ERROR(ret, "ABT_xstream_free");
  }

  /* Free scheds */
  for (int i = 1; i < Impl::g_num_xstreams; i++) {
    ABT_sched_free(&Impl::g_scheds[i]);
  }

  /* Free barrier */
  ABT_barrier_free(&Impl::g_barrier);

  /* Free mutex */
  ABT_mutex_free(&Impl::g_mutex);

  ret = ABT_finalize();
  HANDLE_ERROR(ret, "ABT_finalize");

  free(Impl::g_scheds);
  free(Impl::g_xstreams);
  free(Impl::g_pools);

  char filename[20];
  sprintf(filename, "mlog_%d.ignore", rank);
  FILE *fp = fopen(filename, "w+");
  logger_flush(fp);
  fclose(fp);
}

//----------------------------------------------------------------------------

void Argobots::print_configuration( std::ostream & s , const bool verbose )
{
}

}

#else
void KOKKOS_CORE_SRC_ARGOBOTS_EXEC_PREVENT_LINK_ERROR() {}
#endif //KOKKOS_ENABLE_ARGOBOTS
