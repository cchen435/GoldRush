#ifndef _GR_SCHED_H_
#define _GR_SCHED_H_
/**
 * Scheduler implementation
 *
 */
#include "goldrush.h"
#include "gr_phase.h"

#define GR_DEFAULT_SCHEDULING_INTERVAL 1000
#define GR_SCHEDULING_WINDOW_SIZE 1

typedef struct _perf_window {
    int phase_id;
    long long pctr_values[NUM_EVENTS];
} perf_window, *perf_window_t;

typedef int (* gr_sched_init_func) (void *client_data);

typedef int (* gr_sched_fin_func) (void *client_data);

typedef int (* gr_sched_func) (void *client_data);

typedef struct _gr_scheduler {
    char *name;
    gr_sched_init_func init_func;
    gr_sched_fin_func finalize_func;
    gr_sched_func sched_func;
    void *client_data;
} gr_scheduler, *gr_scheduler_t;

int gr_internal_load_scheduler(char *sched_name, int interval);

int gr_finalize_scheduler();

#endif
