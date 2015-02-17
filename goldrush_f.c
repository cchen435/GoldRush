/**
 * Fortran interface of GoldRush public APIs
 *
 */
#include "goldrush.h"

/*
 * Initialize GoldRush runtime library. 
 * Called by both simulation and analysis.
 *
 * Parameter:
 *  comm: MPI communicator of the calling program
 *
 * Return 0 for success and -1 for error.
 */
int gr_init_(MPI_Fint *comm)
{
    MPI_Comm c_comm = MPI_Comm_f2c(*comm);
    return gr_init(c_comm);
}

/*
 * Finalize GoldRush runtime library.
 * Called by both simulation and analysis.
 *
 * Return 0 for success and -1 for error.
 */
int gr_finalize_()
{
    return gr_finalize();
}

/*
 * Set the applicaiton ID.
 */
void gr_set_application_id_(int *id)
{
    gr_set_application_id(*id);
}

/*
 * Get the applicaiton ID.
 */
void gr_get_application_id_(int *id)
{
    gr_get_application_id(id);
}

/* Public API used by simulation code */ 

/*
 * Mark the start of mainloop
 */
int gr_mainloop_start_()
{
    return gr_mainloop_start();
}

/*
 * Mark the end of mainloop
 */
int gr_mainloop_end_()
{
    return gr_mainloop_end();
}

/*
 * Mark the start of a phase. 
 *
 * Parameter:
 *  file: an integer identifying a source file
 *  line: an integer identifying line number in source file
 *
 * Return 0 for success and -1 for error. 
 */
int gr_phase_start_(unsigned long int *file, unsigned int *line)
{
    int rc = gr_phase_start(*file, *line);
    return rc;
}

/*
 * Mark the end of a phase. It must match a gr_phase_start() call.
 *
 * Parameter:
 *  file: an integer identifying a source file
 *  line: an integer identifying line number in source file
 *
 * Return 0 for success and -1 for error. 
 */
int gr_phase_end_(unsigned long int *file, unsigned int *line)
{
    return gr_phase_end(*file, *line);
}

/*
 * Retrieve a list of registered receivers.
 * 
 * Parameter:
 *  receivers: an array of receiver handles
 *  num_receivers: number of receiver handles
 *
 * Return 0 for success and -1 for error.
 */
int gr_get_receivers_()
{
    gr_receiver_t receivers;
    int num_receivers;
    return gr_get_receivers(&receivers, &num_receivers);
}

/*
 * Load a scheduler specified by name
 *
 * Parameter:
 *  sched_name: name of scheduler
 *  interval: scheduling interval in milliseconds
 * 
 * Return 0 for success and -1 for error.
 */
int gr_load_scheduelr_(char *sched_name, int *interval, int sched_name_size)
{
    return gr_load_scheduler(sched_name, *interval);
}
