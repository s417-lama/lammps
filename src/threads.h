#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <abt.h>

#ifndef ENABLE_PREEMPTION
# define ENABLE_PREEMPTION 0
#endif

#define HANDLE_ERROR(ret, msg)                        \
    if (ret != ABT_SUCCESS) {                         \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg); \
        exit(EXIT_FAILURE);                           \
    }

/* extern ABT_pool* g_pools; */
/* extern ABT_pool g_analysis_pool; */
/* extern int g_num_xstreams; */
/* extern int g_num_threads; */
/* extern ABT_barrier g_barrier; */
/* extern ABT_mutex g_mutex; */

template<typename Func>
static inline void parallel_for(char* name, int begin, int end, Func func)
{
  /* int ret; */
  /* int nb = (end - begin + g_num_threads - 1) / g_num_threads; */
  /* ABT_thread threads[g_num_threads]; */
  /* task* ts[g_num_threads]; */

  /* for (int i = 0; i < g_num_threads; i++) { */
  /*   int b = begin + i * nb; */
  /*   int e = begin + (i + 1) * nb; */
  /*   if (e > end) e = end; */
  /*   auto thread_fn = [=]() { */
  /*     /1* int rank; *1/ */
  /*     /1* ABT_xstream_self_rank(&rank); *1/ */
  /*     /1* void *bp = logger_begin_tl(rank); *1/ */
  /*     for (int j = b; j < e; j++) { */
  /*       func(j); */
  /*     } */
  /*     /1* logger_end_tl(rank, bp, name); *1/ */
  /*   }; */
  /*   ts[i] = new callable_task<decltype(thread_fn)>(thread_fn); */
  /*   ABT_thread_attr attr; */
  /*   ABT_thread_attr_create(&attr); */
  /*   ABT_thread_attr_set_preemption_type(attr, ABT_PREEMPTION_DISABLED); */
  /*   ret = ABT_thread_create(g_pools[i % g_num_xstreams], */
  /*                           invoke, */
  /*                           ts[i], */
  /*                           attr, */
  /*                           &threads[i]); */
  /*   HANDLE_ERROR(ret, "ABT_thread_create"); */
  /* } */

  /* for (int i = 0; i < g_num_threads; i++) { */
  /*   ret = ABT_thread_free(&threads[i]); */
  /*   HANDLE_ERROR(ret, "ABT_thread_free"); */
  /*   delete ts[i]; */
  /* } */
}

template<typename Func>
static inline void parallel_region(char *name, Func func)
{
  /* int ret; */
  /* ABT_thread threads[g_num_threads]; */
  /* task* ts[g_num_threads]; */

  /* for (int i = 0; i < g_num_threads; i++) { */
  /*   auto thread_fn = [=]() { */
  /*     ABT_thread self; */
  /*     ABT_thread_self(&self); */
  /*     ABT_thread_set_arg(self, (void *)i); */

  /*     /1* int rank; *1/ */
  /*     /1* ABT_xstream_self_rank(&rank); *1/ */
  /*     /1* void *bp = logger_begin_tl(rank); *1/ */

  /*     func(i); */

  /*     /1* logger_end_tl(rank, bp, name); *1/ */
  /*   }; */
  /*   ts[i] = new callable_task<decltype(thread_fn)>(thread_fn); */
  /*   ABT_thread_attr attr; */
  /*   ABT_thread_attr_create(&attr); */
  /*   ABT_thread_attr_set_preemption_type(attr, ABT_PREEMPTION_DISABLED); */
  /*   ret = ABT_thread_create(g_pools[i % g_num_xstreams], */
  /*                           invoke, */
  /*                           ts[i], */
  /*                           attr, */
  /*                           &threads[i]); */
  /*   HANDLE_ERROR(ret, "ABT_thread_create"); */
  /* } */

  /* for (int i = 0; i < g_num_threads; i++) { */
  /*   ret = ABT_thread_free(&threads[i]); */
  /*   HANDLE_ERROR(ret, "ABT_thread_free"); */
  /*   delete ts[i]; */
  /* } */
}

static inline void abt_barrier() {
  /* ABT_barrier_wait(g_barrier); */
}

static inline void abt_lock() {
  /* ABT_mutex_lock(g_mutex); */
}

static inline void abt_unlock() {
  /* ABT_mutex_unlock(g_mutex); */
}
