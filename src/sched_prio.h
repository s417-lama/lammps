#pragma once

#include <abt.h>

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

  while (1) {
    ABT_pool_pop(pools[0], &unit);
    if (unit != ABT_UNIT_NULL) {
      ABT_xstream_run_unit(unit, pools[0]);
    } else {
      ABT_pool_pop(pools[1], &unit);
      if (unit != ABT_UNIT_NULL) {
        ABT_xstream_run_unit(unit, pools[1]);
      }
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
