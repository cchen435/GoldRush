#ifndef _GOLDRUSH_H_
#define _GOLDRUSH_H_
/**
 * GoldRush runtime system for managing analytics running on compute node.
 * GoldRush consists of the following components:
 * - process suspend/resume
 * - online simulation phase monitoring
 * 
 * written by Fang Zheng (fzheng@cc.gatech.edu) 
 */
#ifdef __cplusplus
extern "C" {
#endif
 
#include <stdint.h> 
#include <sys/types.h>
#include <mpi.h>

#define GR_MAX_PROCS 32
 
enum GR_RECEIVER_STATE {
    GR_RUNNING = 0,
    GR_SUSPENDED = 1,
    GR_NOT_READY = 2,
    GR_FINISHED = 3    
};

typedef struct _gr_receiver {
    int app_id;
    int num_procs;
    pid_t pid[GR_MAX_PROCS];
    enum GR_RECEIVER_STATE state;
} gr_receiver, *gr_receiver_t;

/*
 * Initialize GoldRush runtime library. 
 * Called by both simulation and analysis.
 *
 * Parameter:
 *  comm: MPI communicator of the calling program
 *
 * Return 0 for success and -1 for error.
 */
int gr_init(MPI_Comm comm); 

/*
 * Finalize GoldRush runtime library.
 * Called by both simulation and analysis.
 *
 * Return 0 for success and -1 for error.
 */
int gr_finalize(); 

/*
 * Set the applicaiton ID.
 */
void gr_set_application_id(int id);

/*
 * Get the applicaiton ID.
 */
void gr_get_application_id(int *id);

/* Public API used by simulation code */ 

/*
 * Mark the start of mainloop
 */
int gr_mainloop_start();

/*
 * Mark the end of mainloop
 */
int gr_mainloop_end();

/*
 * Mark the start of a phase. 
 *
 * Parameter:
 *  file: an integer identifying a source file
 *  line: an integer identifying line number in source file
 *
 * Return 0 for success and -1 for error. 
 */
int gr_phase_start(unsigned long int file, unsigned int line);

/*
 * Mark the end of a phase. It must match a gr_phase_start() call.
 *
 * Parameter:
 *  file: an integer identifying a source file
 *  line: an integer identifying line number in source file
 *
 * Return 0 for success and -1 for error. 
 */
int gr_phase_end(unsigned long int file, unsigned int line);

/*
 * Retrieve a list of registered receivers.
 * 
 * Parameter:
 *  receivers: an array of receiver handles
 *  num_receivers: number of receiver handles
 *
 * Return 0 for success and -1 for error.
 */
int gr_get_receivers(gr_receiver_t *receivers, int *num_receivers);

/*
 * Retrieve the receiver handle by the name of the data group to which
 * the receiver is subsribing
 *
 * Parameter:
 *  data_group_name: name of the data group
 *
 * Return handle of receiver for success and NULL for error.
 */
gr_receiver_t gr_get_receiver_by_data_group(char *data_group_name);

/*
 * Suspend a receiver's execution. All processes of the receiver
 * is suspended upon return of this function.
 * 
 * Parameter:
 *  receiver: handle to the receiver 
 *
 * Return 0 for success and -1 for error.
 */
int gr_suspend_receiver(gr_receiver_t receiver);

/*
 * Resume the execution of the specified receiver. All processes of the
 * receiver is resumed upon return of this function. 
 * 
 * Parameter:
 *  receiver: handle to the receiver 
 *
 * Return 0 for success and -1 for error.
 */
int gr_resume_receiver(gr_receiver_t receiver);

/* Public API used by analysis code */

/*
 * Load a scheduler specified by name
 *
 * Parameter:
 *  sched_name: name of scheduler
 *  interval: scheduling interval in milli-seconds
 *
 * Return 0 for success and -1 for error.
 */
int gr_load_scheduler(char *sched_name, int interval);

/*
 * Register the calling program as a receiver of the data group.
 *
 * Parameter:
 *  data_group_name: name of the data group
 *  comm: MPI communicator of the calling program
 *
 * Return 0 for success and -1 for error.
 */
int gr_register_receiver(char *data_group_name, MPI_Comm comm);

/* Fortran API */

int gr_init_(MPI_Fint *comm);

int gr_finalize_();

void gr_set_application_id_(int *id);

void gr_get_application_id_(int *id);

int gr_mainloop_start_();

int gr_mainloop_end_();

int gr_phase_start_(unsigned long int *file, unsigned int *line);

int gr_phase_end_(unsigned long int *file, unsigned int *line);

int gr_load_scheduelr_(char *sched_name, int *interval, int sched_name_size);

int gr_get_receivers_();

#ifdef __cplusplus
}
#endif

#endif
