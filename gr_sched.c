/**
 * Scheduler implementation
 *
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include "df_shm.h"
#include "gr_monitor_buffer.h"
#include "gr_perfctr.h"
#include "gr_sched.h"
#include "gr_internal.h"

// Global variables
int scheduling_interval_us = GR_DEFAULT_SCHEDULING_INTERVAL;
int num_read_lock_tries = 10;
volatile sig_atomic_t disable_scheduler = 0;
struct sigaction old_sa;
gr_scheduler gr_global_scheduler = {
    .name = NULL, 
    .init_func = NULL,
    .finalize_func = NULL,
    .sched_func = NULL,
    .client_data = NULL
};

// performance monitor window for simulation
perf_window_t perf_windows = NULL;
int perf_window_size = GR_SCHEDULING_WINDOW_SIZE;
int perf_window_idx = 0;

perf_window_t self_perf_windows = NULL;
int self_perf_window_size = GR_SCHEDULING_WINDOW_SIZE;
int self_perf_window_idx = 0;

long long pctr_v1[NUM_EVENTS];
long long pctr_v2[NUM_EVENTS];
long long *cur_perfctr, *old_perfctr;

extern df_shm_method_t gr_shm_handle;
extern df_shm_region_t gr_mon_buffer_region;
extern gr_mon_buffer_t gr_monitor_buffer;
extern int gr_local_rank;
extern int gr_local_size;
extern int gr_comm_rank;

// Forward definitions
int gr_greedy_sched_init(void *client_data);
int gr_greedy_sched_finialize(void *client_data);
int gr_greedy_sched_func(void *client_data);

typedef struct _contenion_sched_param {
    double ipc_threshold;
    double l2_miss_threshold;
    double sleep_duration;
} contention_sched_param, *contention_sched_param_t;

int gr_contention_sched_init(void *client_data);
int gr_contention_sched_finialize(void *client_data);
int gr_contention_sched_func(void *client_data);

#ifdef DEBUG_TIMING
#include "rdtsc.h"

typedef struct _sched_trace {
    long long timestamp;
    int phase_id;
    int duration;
    long long sim_cycle;
    long long sim_inst;
    long long l2_miss;
    long long analysis_cycle;
} sched_trace, *sched_trace_t;

#define TRACE_SIZE 100000
sched_trace sched_traces[TRACE_SIZE];
int sched_trace_idx = 0;
FILE *sched_tracefile = NULL;

void dump_sched_trace()
{
    int i;
    for(i = 0; i < sched_trace_idx; i ++) {
        fprintf(sched_tracefile, "%d\t%lld\t%lld\t%lld\t%lld\t%lld\t%d\n",
                sched_traces[i].phase_id,
                sched_traces[i].timestamp,
                sched_traces[i].sim_cycle,
                sched_traces[i].sim_inst,
                sched_traces[i].l2_miss,
                sched_traces[i].analysis_cycle,
                sched_traces[i].duration
               );
    }
    sched_trace_idx = 0;
}

#endif

int gr_delay_usec(int usec)
{
    // wait for usec micro-seconds
/*
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = usec*1000;
    naosleep(&ts, NULL);
*/
    usleep(usec);
    return 0;
}

void gr_timer_sched_handler(int signum)
{
    if(disable_scheduler) {
        return;
    }

    int i;
    // get performance data of this process
    gr_perfctr_read(cur_perfctr);
    long long *window = self_perf_windows[self_perf_window_idx].pctr_values;
    for(i = 0; i < NUM_EVENTS; i ++) {
        window[i] = cur_perfctr[i] - old_perfctr[i];
    }
    self_perf_window_idx = (self_perf_window_idx+1) % self_perf_window_size;
    long long *temp_p = cur_perfctr;
    cur_perfctr = old_perfctr;
    old_perfctr = temp_p;

    int rc = 0;
    int phase_id = 0;
    // read performance counters from monitor buffer
    // try to lock monitor buffer
    for(i = 0; i < num_read_lock_tries; i ++) {
        rc = pthread_rwlock_tryrdlock(&gr_monitor_buffer->rwlock);
        if(rc == 0) {
            // have locked monitor buffer
            phase_id = gr_monitor_buffer->phase_id;
            int j;
            long long *dest = perf_windows[perf_window_idx].pctr_values;
            long long *src = gr_monitor_buffer->perfctr_values;
            perf_windows[perf_window_idx].phase_id = perf_windows[perf_window_idx].phase_id;
            for(j = 0; j < NUM_EVENTS; j ++) {
                dest[j] = src[j];
            }              
            perf_window_idx = (perf_window_idx+1) % perf_window_size;

            pthread_rwlock_unlock(&gr_monitor_buffer->rwlock);
            break;
        }
    }

    // invoke scheduler function
    rc = (*gr_global_scheduler.sched_func) (NULL);

    if(rc == 0) {
        // let analytics running
        // do nothing
    }
    else if(rc >0) {
        // let analytics wait for rc micro-seconds
        gr_delay_usec(rc);
    }
    else { 
        // error happenned
        // do nothing for now
    }

#ifdef DEBUG_TIMING
    if(sched_trace_idx == TRACE_SIZE) {
        dump_sched_trace();    
    }

    sched_traces[sched_trace_idx].phase_id = phase_id;
    sched_traces[sched_trace_idx].timestamp = rdtsc();
    sched_traces[sched_trace_idx].sim_cycle = perf_windows[perf_window_idx-1].pctr_values[0]; 
    sched_traces[sched_trace_idx].sim_inst = perf_windows[perf_window_idx-1].pctr_values[1];
    sched_traces[sched_trace_idx].l2_miss = self_perf_windows[self_perf_window_idx-1].pctr_values[2]; 
    sched_traces[sched_trace_idx].analysis_cycle = self_perf_windows[self_perf_window_idx-1].pctr_values[0];
    sched_traces[sched_trace_idx].duration = rc;
    sched_trace_idx ++; 
#endif

    // re-install timer
    struct itimerval it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = scheduling_interval_us;
    setitimer(ITIMER_REAL, &it, NULL);
}

static int gr_register_scheduler(gr_scheduler_t sched_handle,
                                 char *name,
                                 gr_sched_init_func init_func,
                                 gr_sched_fin_func finalize_func,
                                 gr_sched_func sched_func,
                                 void *client_data
                                )
{
    sched_handle->name = strdup(name);
    sched_handle->init_func = init_func;
    sched_handle->finalize_func = finalize_func;
    sched_handle->sched_func = sched_func;
    sched_handle->client_data = client_data;
    if(init_func) {
        return (*init_func) (client_data);
    }
    return 0;
} 

int gr_internal_load_scheduler(char *sched_name, int interval)
{
    int rc;
    scheduling_interval_us = interval * 1000; // convert to microsecond

    // set up the scheduler function
    if(!strcmp(sched_name, "greedy")) {
        rc = gr_register_scheduler(&gr_global_scheduler,
                                   "greedy",
                                   gr_greedy_sched_init, 
                                   gr_greedy_sched_finialize,
                                   gr_greedy_sched_func,
                                   NULL
                                  );
    }
    else if(!strcmp(sched_name, "contention")) {
        contention_sched_param_t param = (contention_sched_param_t) 
            malloc(sizeof(contention_sched_param));
        if(!param) {
            fprintf(stderr, "Error: cannot allocate memory. %s:%d\n", 
                __FILE__, __LINE__);
            return -1;
        }
        char *temp_str = getenv("GR_SCHED_IPC_THRESHOLD");
        if(temp_str) {
            param->ipc_threshold = (double) atoi(temp_str);
        }
        else {
            param->ipc_threshold = 1;
        }

        temp_str = getenv("GR_SCHED_L2MISS_THRESHOLD");
        if(temp_str) {
            param->l2_miss_threshold = (double) atoi(temp_str);
        }
        else {
            param->l2_miss_threshold = 10;
        }

        temp_str = getenv("GR_SCHED_SLEEP");
        if(temp_str) {
            param->sleep_duration = (double) atoi(temp_str);
        }
        else {
            param->sleep_duration = scheduling_interval_us * 0.2;
        }

        rc = gr_register_scheduler(&gr_global_scheduler,
                                   "contention",
                                   gr_contention_sched_init,
                                   gr_contention_sched_finialize,
                                   gr_contention_sched_func,
                                   param
                                  );
    }
    else if(sched_name == NULL || !strcmp(sched_name, "default")) {
        fprintf(stderr, "Disable scheduler\n");
        return 0;
    }
    else {
        fprintf(stderr, "Error: unknown scheduler %s.\n", sched_name);
        return -1;
    }
    if(rc) {
        fprintf(stderr, "Error: loading scheduler %s returns %d. %s:%d\n",
            sched_name, rc, __FILE__, __LINE__);
        return -1;
    }

    // attach to simulation monitor buffer
    // get sender contact info
    gr_sender_t sim;
    while(1) {
        sim = gr_get_sender(0); 
        if(sim) break;
        sleep(1);
    }
    int sim_rank = gr_local_rank / sim->num_procs;

fprintf(stderr, "analysis comm rank %d local rank %d sim rank %d\n", gr_comm_rank, gr_local_rank, sim_rank);

    key_t sim_shm_key = sim->shm_mon_buffer_key[sim_rank];
    gr_mon_buffer_region = gr_attach_monitor_buffer(gr_shm_handle, sim_shm_key);
    gr_monitor_buffer = (gr_mon_buffer_t) gr_mon_buffer_region->starting_addr;

    char *ws_str = getenv("GR_SCHED_WINDOW_SIZE");
    if(ws_str) {
        perf_window_size = atoi(ws_str);
    }
    perf_windows = (perf_window_t) malloc(perf_window_size * sizeof(perf_window)); 
    if(!perf_windows) {
        fprintf(stderr, "Error: cannot allocate memory. %s:%d\n",
            __FILE__, __LINE__);
        return -1;
    }
    perf_window_idx = 0;
    self_perf_window_size = perf_window_size;
    self_perf_windows = (perf_window_t) malloc(self_perf_window_size * sizeof(perf_window));
    if(!self_perf_windows) {
        fprintf(stderr, "Error: cannot allocate memory. %s:%d\n",
            __FILE__, __LINE__);
        return -1;
    }
    self_perf_window_idx = 0;

    // establish signal handler
    struct sigaction psa;
    psa.sa_handler = gr_timer_sched_handler;
    sigemptyset(&psa.sa_mask);
    psa.sa_flags = SA_RESTART;
    rc = sigaction(SIGALRM, &psa, &old_sa);
    if(rc != 0) {
        fprintf(stderr, "Error: sigaction() returns %d\n", rc);
        perror("sigaction");
        return -1;
    }

#ifdef DEBUG_TIMING
    char trace_filename[50];
    sprintf(trace_filename, "sched_trace.%d\0", gr_comm_rank);
    sched_tracefile = fopen(trace_filename, "w");
#endif

    // read initial performance counters
    cur_perfctr = pctr_v1;
    old_perfctr = pctr_v2;
    gr_perfctr_read(cur_perfctr);

    // setup timer
    struct itimerval it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = scheduling_interval_us;
    setitimer(ITIMER_REAL, &it, NULL);
    return 0;
}

int gr_finalize_scheduler()
{
    int rc;

    if(gr_global_scheduler.name == NULL) {
        return 0;
    }

    // diable timer and signal handler
    disable_scheduler = 1;
    sigaction(SIGALRM, &old_sa, NULL);

    if(gr_global_scheduler.finalize_func) {
        rc = (*gr_global_scheduler.finalize_func)(gr_global_scheduler.client_data);
        if(rc) {
            return rc;
        }
    }
    free(gr_global_scheduler.name);
    free(perf_windows);
    free(self_perf_windows);
    rc = gr_destroy_monitor_buffer(gr_mon_buffer_region);

#ifdef DEBUG_TIMING
    dump_sched_trace();
    fclose(sched_tracefile);
#endif
    return rc;
}

/*
 * The greedy scheduler runs analysis on every available phase
 */
int gr_greedy_sched_init(void *client_data)
{
    return 0;
}

int gr_greedy_sched_finialize(void *client_data)
{
    return 0;
}

int gr_greedy_sched_func(void *client_data)
{
    // be greedy: always let analytics continue running
    return 0;
}

/*
 * The contention-aware scheduler only runs analysis during a phase if the estimated
 * contention is acceptable
 */
int gr_contention_sched_init(void *client_data)
{
    return 0;
}

int gr_contention_sched_finialize(void *client_data)
{
    free(client_data);
    return 0;
}

int gr_contention_sched_func(void *client_data)
{
    contention_sched_param_t param = (contention_sched_param_t) client_data;

    // use a contention model to decide
    // 1. whether simulation is suffering from contention
    // 2. whether this process is causing the contention
    long long num_cycles = perf_windows[perf_window_idx-1].pctr_values[0];    
    long long num_intrs = perf_windows[perf_window_idx-1].pctr_values[1];    
    double ipc = num_intrs / num_cycles;

    long long *window = self_perf_windows[self_perf_window_idx-1].pctr_values;
    double l2_miss_rate = window[2] / window[0] * 1000;

    if(ipc < param->ipc_threshold) {
        if(l2_miss_rate > param->l2_miss_threshold) {
            return (int) param->sleep_duration;
        }
    }

    return 0;
}

