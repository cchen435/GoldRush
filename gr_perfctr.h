#ifndef _GR_PERFCTR_H_
#define _GR_PERFCTR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define NUM_EVENTS 4

typedef struct _gr_perfctr {
    long long count;
    long long start_values[NUM_EVENTS];
    long long avg_values[NUM_EVENTS];
    long long min_values[NUM_EVENTS];
    long long max_values[NUM_EVENTS];
} gr_perfctr, *gr_perfctr_t;

/*
 * Initialize hardware performance counter monitoring.
 */ 
int gr_perfctr_init(int mpi_rank);

/*
 * Finalize performance counter monitoring.
 */
int gr_perfctr_finalize(int mpi_rank);

/*
 * Start performance counter monitoring.
 */
int gr_perfctr_start(int mpi_rank);

/*
 * Stop performance counter monitoring.
 */
int gr_perfctr_stop(int mpi_rank);

/*
 * Test if performance counter monitoring is on of off.
 */
int gr_perfctr_is_on();

/*
 * Set the initial values for a performance counter handle.
 */
int gr_perfctr_init_counter(gr_perfctr_t counter);

/*
 * Read performance counter values.
 */
int gr_perfctr_read(long long *values);

/*
 * Read performance counter values at the start of a phase.
 */
int gr_perfctr_phase_start(gr_perfctr_t counter);

/*
 * Read performance counter values at the end of a phase.
 */
int gr_perfctr_phase_end(gr_perfctr_t counter);

/*
 * Update the counter with specified values
 */
void gr_perfctr_update(gr_perfctr_t counter, long long *pctr_values);

/*
 * Print out performance counter results
 */
void gr_perfctr_print(FILE *log_file, gr_perfctr_t c, int phase_id);

#ifdef __cplusplus
}
#endif

#endif

