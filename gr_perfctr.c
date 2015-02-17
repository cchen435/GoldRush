#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "papi.h"
#include "gr_perfctr.h"

/*
 * hardware performance counter events to monitor
 */
static int *gr_PAPI_events = NULL;
static char **gr_PAPI_enames = NULL;
static int gr_num_events = 0;

static int gr_PAPI_eventset = PAPI_NULL ;
static int is_counting = 0;

/* Forward declarations */
int gr_perfctr_start(int mpi_rank);
int gr_perfctr_stop(int mpi_rank);

/*
 * Initialize hardware performance counter monitoring.
 */ 
int gr_perfctr_init(int mpi_rank)
{
    int rc = PAPI_library_init(PAPI_VER_CURRENT);
    if(rc != PAPI_VER_CURRENT) {
        fprintf(stderr, "Error: rank %d PAPI error: %d %d:%s. %s:%d\n", 
            mpi_rank, rc, PAPI_VER_CURRENT, PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }
 
    rc = PAPI_create_eventset(&gr_PAPI_eventset); 
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: rank %d PAPI error: %s. %s:%d\n", 
            mpi_rank, PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }

    // pass environment variable "GR_PERFCTR_EVENTS" to get events
    char *gr_perfctr_events = getenv("GR_PERFCTR_EVENTS");
    while(gr_perfctr_events && *gr_perfctr_events != '\0') {
        char *temp_str = strchr(gr_perfctr_events, ';');
        if(temp_str) {
            *temp_str = '\0';
            temp_str ++;
        }
        int event_code;
        rc = PAPI_event_name_to_code(gr_perfctr_events, &event_code);    
        if(rc != PAPI_OK) {
            fprintf(stderr, "Error: rank %d PAPI error: %s. %s:%d\n",
                mpi_rank, PAPI_strerror(rc), __FILE__, __LINE__);
            return -1;
        }
        rc = PAPI_add_event(gr_PAPI_eventset, event_code);
        if(rc != PAPI_OK) {
            fprintf(stderr, "Error: rank %d PAPI error: %s. %s:%d\n", 
                mpi_rank, PAPI_strerror(rc), __FILE__, __LINE__);
            return -1;
        }
        if(!gr_PAPI_events) {
            gr_PAPI_events = (int *) malloc(sizeof(int)); 
            if(!gr_PAPI_events) {
                fprintf(stderr, "Error: cannot allocate memory. %s:%d\n",
                    __FILE__, __LINE__);
                return -1;
            } 
        }         
        else {
            gr_PAPI_events = (int *) realloc(gr_PAPI_events, (gr_num_events+1)*sizeof(int));
            if(!gr_PAPI_events) {
                fprintf(stderr, "Error: cannot allocate memory. %s:%d\n",
                    __FILE__, __LINE__);
                return -1;
            }
        }
        if(!gr_PAPI_enames) {
            gr_PAPI_enames = (char **) malloc(sizeof(char *));
            if(!gr_PAPI_enames) {
                fprintf(stderr, "Error: cannot allocate memory. %s:%d\n",
                    __FILE__, __LINE__);
                return -1;
            }
        }
        else {
            gr_PAPI_enames = (char **) realloc(gr_PAPI_enames, (gr_num_events+1)*sizeof(char *));
            if(!gr_PAPI_enames) {
                fprintf(stderr, "Error: cannot allocate memory. %s:%d\n",
                    __FILE__, __LINE__);
                return -1;
            }
        }
fprintf(stderr, "event %s\n", gr_perfctr_events);
        gr_PAPI_events[gr_num_events] = event_code;
        gr_PAPI_enames[gr_num_events] = strdup(gr_perfctr_events);
        gr_num_events ++;
        gr_perfctr_events = temp_str;
    }
    return 0;
}

/*
 * Finalize performance counter monitoring.
 */
int gr_perfctr_finalize(int mpi_rank)
{
    // stop counting
    gr_perfctr_stop(mpi_rank);

    int rc = PAPI_cleanup_eventset(gr_PAPI_eventset);
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: rank %d PAPI error: %s. %s:%d\n", 
            mpi_rank, PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }
    rc = PAPI_destroy_eventset(&gr_PAPI_eventset);    
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: rank %d PAPI error: %s. %s:%d\n", 
            mpi_rank, PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }
    PAPI_shutdown();

    free(gr_PAPI_events);
    int i;
    for(i = 0; i < gr_num_events; i ++) {
        free(gr_PAPI_enames[i]);
    }
    free(gr_PAPI_enames);
    return 0;
}

/*
 * Start performance counter monitoring.
 */
int gr_perfctr_start(int mpi_rank)
{
    if(is_counting) {
        return 0;
    }
    int rc = PAPI_start(gr_PAPI_eventset);
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: rank %d PAPI error: %s. %s:%d\n", 
            mpi_rank, PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }
    is_counting = 1;
    return 0;
}

/*
 * Stop performance counter monitoring.
 */
int gr_perfctr_stop(int mpi_rank)
{
    if(!is_counting) {
        return 0;
    }
    
    long long values[gr_num_events];
    int rc = PAPI_stop(gr_PAPI_eventset, values);
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: rank %d PAPI error: %s. %s:%d\n", 
            mpi_rank, PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }
    is_counting = 0;
    return 0;
}

/*
 * Test if performance counter monitoring is on of off.
 */
int gr_perfctr_is_on()
{
    return is_counting == 1;
}

/*
 * Set the initial values for a performance counter handle.
 */
int gr_perfctr_init_counter(gr_perfctr_t counter)
{
    int i;
    for(i = 0 ; i < gr_num_events; i ++) {
        counter->avg_values[i] = 0;
        counter->min_values[i] = -1;
        counter->max_values[i] = -1;
    }
    counter->count = 0;
    return 0;
}

/*
 * Read performance counter values.
 */
int gr_perfctr_read(long long *values)
{
    int rc = PAPI_read(gr_PAPI_eventset, values);
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: PAPI error: %s. %s:%d\n", 
            PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }  
    return 0;
}

/*
 * Read performance counter values at the start of a phase
 */
int gr_perfctr_phase_start(gr_perfctr_t counter)
{
    int rc = PAPI_read(gr_PAPI_eventset, counter->start_values);
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: PAPI error: %s. %s:%d\n",
            PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

/*
 * Read performance counter values at the end of a phase
 */
int gr_perfctr_phase_end(gr_perfctr_t counter)
{
    long long values[gr_num_events];
    int rc = PAPI_read(gr_PAPI_eventset, values);
    if(rc != PAPI_OK) {
        fprintf(stderr, "Error: PAPI error: %s. %s:%d\n",
            PAPI_strerror(rc), __FILE__, __LINE__);
        return -1;
    }
    counter->count ++;
    int i;
    for(i = 0; i < gr_num_events; i ++) {
        long long t = values[i] - counter->start_values[i];
        counter->avg_values[i] += t;
        if(t < counter->min_values[i] || counter->min_values[i] == -1) {
            counter->min_values[i] = t;
        }
        if(t > counter->max_values[i]) {
            counter->max_values[i] = t;
        }
    }
    return 0;
}

/*
 * Update the counter with specified values
 */
void gr_perfctr_update(gr_perfctr_t counter, long long *pctr_values)
{
    counter->count ++;
    int i;
    for(i = 0; i < gr_num_events; i ++) {
        counter->avg_values[i] += pctr_values[i];
        if(pctr_values[i] < counter->min_values[i] || counter->min_values[i] == -1) {
            counter->min_values[i] = pctr_values[i];
        }
        if(pctr_values[i] > counter->max_values[i]) {
            counter->max_values[i] = pctr_values[i];
        }
    }
}

/*
 * Print out performance counter results
 */
void gr_perfctr_print(FILE *log_file, gr_perfctr_t c, int phase_id)
{
    int i;
    for(i = 0; i < gr_num_events; i ++) {
        fprintf(log_file, "%d\t%d\t%s\t%lld\t%lld\t%lld\n",
            phase_id,
            c->count,
            gr_PAPI_enames[i],
            c->max_values[i],
            c->min_values[i],
            (c->count == 0)? 0:c->avg_values[i]/c->count
        );
    }
}

