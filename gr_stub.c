#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "gr_monitor_buffer.h"
#include "gr_perfctr.h"
#include "gr_stub.h"

/* Tunable parameters */
int timer_interval_us;  // timer interval in micro-seconds 
int num_lock_tries;     // number of locking attempts on monitor buffer

/* Global data */
volatile sig_atomic_t disable_handler = 0;
struct sigaction old_sa;
long long perfctr_values1[NUM_EVENTS];
long long perfctr_values2[NUM_EVENTS];
long long *current_pctr, *old_pctr;
struct itimerval start_t;
struct itimerval end_t;

extern gr_mon_buffer_t gr_monitor_buffer;
extern current_phase_id;

void gr_timer_handler(int signum)
{
//printf("TIMER: im here %s %d\n", __FILE__, __LINE__);
    if(disable_handler) {
        return;
    }

    int rc = 0;
    // read performance counters into temp buffer
    gr_perfctr_read(current_pctr);
    int j;
    for(j = 0; j < NUM_EVENTS; j ++) {
        old_pctr[j] = current_pctr[j] - old_pctr[j]; 
    }
         
    // try to lock monitor buffer
    int i = 0;
    while(i < num_lock_tries) {
        rc = pthread_rwlock_trywrlock(&gr_monitor_buffer->rwlock);
        if(rc == 0) {
            // have locked monitor buffer
            // update shared memory monitor buffer
            gr_monitor_buffer->phase_id = current_phase_id;
            long long *dest = gr_monitor_buffer->perfctr_values; 
            for(j = 0; j < NUM_EVENTS; j ++) {
                dest[j] = old_pctr[j];
            }
            pthread_rwlock_unlock(&gr_monitor_buffer->rwlock); 
            break;
        } 
        i ++;
    }    
    long long *temp_p = current_pctr;   
    current_pctr = old_pctr;   
    old_pctr = temp_p;

    // re-install timer
    setitimer(ITIMER_REAL, &start_t, NULL);
//printf("TIMER: end im here %s %d\n", __FILE__, __LINE__);
}

int gr_stub_init(int timer_interval, int num_locking)
{
    // initialize monitor buffer
    timer_interval_us = timer_interval;
    num_lock_tries = num_locking;

    start_t.it_interval.tv_sec = 0;
    start_t.it_interval.tv_usec = 0;
    start_t.it_value.tv_sec = 0;
    start_t.it_value.tv_usec = timer_interval_us;

    end_t.it_interval.tv_sec = 0;
    end_t.it_interval.tv_usec = 0;
    end_t.it_value.tv_sec = 0;
    end_t.it_value.tv_usec = 0;

    // establish signal handler
    struct sigaction psa;
    psa.sa_handler = gr_timer_handler;
    sigemptyset(&psa.sa_mask);
    psa.sa_flags = SA_RESTART;
    int rc = sigaction(SIGALRM, &psa, &old_sa);
    if(rc != 0) {
        fprintf(stderr, "Error: sigaction() returns %d\n", rc);
        perror("sigaction");
        return -1;
    }
    return 0;
}

int gr_stub_finalize()
{
    disable_handler = 1;
    int rc = sigaction(SIGALRM, &old_sa, NULL);
    return rc;
}

int gr_stub_phase_start(long long *ptr)
{
    // read performance counters into temp buffer
    old_pctr = perfctr_values1; 
    current_pctr = perfctr_values2;
    // here we try to avoid reading perf counter twice
    int j;
    for(j = 0; j < NUM_EVENTS; j ++) {
        old_pctr[j] = ptr[j];
    } 
    disable_handler = 0;

    // setup timer
    setitimer(ITIMER_REAL, &start_t, NULL);
    return 0;
}

int gr_stub_phase_end()
{
    // disable timer
    disable_handler = 1;
    setitimer(ITIMER_REAL, &end_t, NULL);
    return 0;
}

