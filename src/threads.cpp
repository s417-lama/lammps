#include "threads.h"

ABT_pool* g_pools;
int g_num_xstreams;
int g_num_threads;
ABT_barrier g_barrier;
ABT_mutex g_mutex;
