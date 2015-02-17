/*
 * A program to monitor hardware Performance counter.
 */
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <mpi.h>
#include "goldrush.h"
#include "gr_perfctr.h"

// performance buffer
#define PERF_BUFFER_SIZE 1000000
typedef struct _perf_buffer {
    double wtime;
    gr_perfctr counter;
} perf_buf;
static perf_buf perf_buffer[PERF_BUFFER_SIZE];
static unsigned int perf_idx = 0;

int procs_per_node = 4;
int mpi_rank;
int mpi_size;

int default_sampling_interval = 1000000; // 1 milliseconds
int default_total_time = 20; // 20 second

double probe_wtime()
{
    struct timeval tp;
    struct timezone tzp;
    gettimeofday(&tp,&tzp);
    return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

int main(int argc, char* argv[])
{
    int rc;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    // set CPU affinity
    int coreid = (mpi_rank % procs_per_node) * procs_per_node;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(coreid, &mask);
    if(!sched_setaffinity(0, sizeof(mask), &mask)) {
        fprintf(stderr, "GR_PERF_PROBE: set rank %d to core %d.\n",
            mpi_rank, coreid);
    }
    else {
        fprintf(stderr, "cannot PI set to core %d\n", coreid);
    }

    // parse commandline parameter
    int interval = default_sampling_interval;
    int total_time = default_total_time;
    if(argc != 3) {
        if(mpi_rank == 0) {
            fprintf(stderr, "usage: %s interval total_time\n", argv[0]);
        }
        exit(-1);
    }
    else { 
        interval = atoi(argv[1]);
        total_time = atoi(argv[2]);
    }
    if(mpi_rank == 0) {
        fprintf(stderr, "GR_PEERF_PROBE: sampling interval: %f ms.\n",
        interval / 1.0e6);
    }

    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = interval;
   
    char log_file_name[20];
    sprintf(log_file_name, "gr_probe.%d\0", mpi_rank);
    FILE *log_file = fopen(log_file_name, "w");
    if(!log_file) {
        fprintf(stderr, "cannot open file %s\n", log_file_name);
    }

    rc = gr_perfctr_init(mpi_rank);
    rc = gr_perfctr_start(mpi_rank);

    perf_idx = 0;
    double start_time = probe_wtime();
    fprintf(log_file, "start:%f\n", start_time);
    // main loop
    while(1) {
        perf_buffer[perf_idx].wtime = probe_wtime();
        gr_perfctr_t counter = &perf_buffer[perf_idx].counter;
        gr_perfctr_phase_start(counter);           
        
        nanosleep(&req, NULL);

        gr_perfctr_phase_end(counter);

        perf_idx ++;   
        if(perf_idx >= PERF_BUFFER_SIZE) {
            // flush perforamnce buffer
            int i;
            for(i = 0; i < PERF_BUFFER_SIZE; i ++) {
                fprintf(log_file, "%d\t%f\n", i, perf_buffer[i].wtime);
                gr_perfctr_print(log_file, &perf_buffer[i].counter, i);
            }
            fflush(log_file);
            perf_idx = 0;
        }  
        double current_time = probe_wtime();
        if((current_time - start_time) >= total_time) {
            break;
        }
    }

    int i;
    for(i = 0; i < perf_idx; i ++) {
        fprintf(log_file, "%d\t%f\n", i, perf_buffer[i].wtime);
        gr_perfctr_print(log_file, &perf_buffer[i].counter, i);
    }
    fflush(log_file);
    fclose(log_file);

    rc = gr_perfctr_finalize(mpi_rank);
    MPI_Finalize();
    return rc;
}

