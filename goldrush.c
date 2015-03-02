/**
 * GoldRush runtime system for managing analytics running on compute node.
 * GoldRush consists of the following components:
 * - process suspend/resume
 * - online simulation phase monitoring
 * 
 * written by Fang Zheng (fzheng@cc.gatech.edu) 
 */
#include <stdint.h> 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <signal.h>
#include <semaphore.h>
#include <mpi.h>
#include "df_shm.h"
#include "goldrush.h"
#include "rdtsc.h"
#include "gr_internal.h"
#ifdef GR_HAVE_PERFCTR
#include "gr_perfctr.h"
#include "gr_monitor_buffer.h"
#include "gr_stub.h"
#endif
#include "gr_phase.h"

/* changed by Chao for kitten, using kitten scheduler API 
   for suspend operation 
*/
#include "coopsched.h"

/* Macros and constants */
#define CHOSEN_SHM_METHOD DF_SHM_METHOD_SYSV

/* Global Variable */
int gr_app_id;
MPI_Comm gr_comm;
int gr_comm_rank;
int gr_comm_size;
int gr_local_rank;
int gr_local_size;
int is_simulation;

unsigned long int current_phase_file;
unsigned int current_phase_line;
uint64_t current_phase_start_time;
long long current_phase_perfctr_values[NUM_EVENTS];
int current_phase_id = 0;
int is_resumed = 0;
int has_start_phase = 0;
int is_in_mainloop = 0;


df_shm_method_t gr_shm_handle = NULL;
df_shm_region_t gr_shm_meta_region = NULL;
gr_shm_layout_t gr_shm_meta = NULL;
#ifdef GR_HAVE_PERFCTR
df_shm_region_t gr_mon_buffer_region = NULL;
gr_mon_buffer_t gr_monitor_buffer = NULL;
#endif

/* configuration constants */
key_t gr_meta_region_key = 1997;
char *gr_meta_region_mmap_name = "/tmp/gr_meta_region";
char *gr_meta_region_posix_name = "/gr_meta_region";

int gr_do_suspend = 1;
int gr_do_phase_perfctr = 1;
int min_phase_length = 0; //2000083; // 1 ms for smoky
int gr_do_stub = 1;

#ifdef DEBUG_TIMING
/* dump timing results */
int my_rank;
FILE *log_file;
char log_file_name[30];
uint64_t t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
uint64_t total_time = 0;
uint64_t monitor_time = 0;
uint64_t perfctr_time = 0;
uint64_t suspend_time = 0;
uint64_t schedule_time = 0;
uint64_t stub_time = 0;
uint64_t resume_time = 0;
uint64_t phase_time = 0;
uint64_t call_count = 0;
#endif

/*
 * Set the applicaiton ID.
 */
void gr_set_application_id(int id)
{
    gr_app_id = id;
}

/*
 * Get the applicaiton ID.
 */
void gr_get_application_id(int *id)
{
    *id = gr_app_id;
}

/*
 * Initialize GoldRush runtime library. 
 * Called by both simulation and analysis.
 *
 * Parameter:
 *  comm: MPI communicator of the calling program
 *
 * Return 0 for success and -1 for error.
 */
int gr_init(MPI_Comm comm)
{
    gr_comm = comm;
    MPI_Comm_rank(comm, &gr_comm_rank);
    MPI_Comm_size(comm, &gr_comm_size);
    gr_local_rank = gr_get_local_rank();
    gr_local_size = gr_get_num_procs_per_node(comm);



    /**
     * creating a shared memory, as in kitten, possibly not using
     * shared memory stuff, so commented here
      */
 #if 0
    char *shm_method_str = getenv("DF_SHM_METHOD");
    enum DF_SHM_METHOD shm_method_no;
    if(shm_method_str) {
        shm_method_no = atoi(shm_method_str);
    }
    else {
        shm_method_no = CHOSEN_SHM_METHOD;
    }
    gr_shm_handle = df_shm_init(shm_method_no, NULL);
    
    void *meta_region_name;
    int meta_region_name_size;
    if(shm_method_no == DF_SHM_METHOD_SYSV) {
        meta_region_name = (void *) &gr_meta_region_key;
        meta_region_name_size = sizeof(gr_meta_region_key);
    }
    else if(shm_method_no == DF_SHM_METHOD_MMAP) {
        meta_region_name = (void *) gr_meta_region_mmap_name;
        meta_region_name_size = strlen(gr_meta_region_mmap_name)+1;
    }
    else {
        meta_region_name = (void *) gr_meta_region_posix_name;
        meta_region_name_size = strlen(gr_meta_region_posix_name)+1;
    }
    int meta_region_size = gr_get_shm_meta_region_size();

    // initialize and attach to shared memory meta-data region
    if(gr_is_local_leader()) {
        // create shm meta region at a well-known place
        gr_shm_meta_region = df_create_named_shm_region(gr_shm_handle, 
            meta_region_name, meta_region_name_size, meta_region_size, NULL);
        if(!gr_shm_meta_region) {
            fprintf(stderr, "Error: Cannot create region. %s:%d\n", 
                __FILE__, __LINE__);
            exit(-1);
        }
        if(gr_init_shm_meta_region(gr_shm_meta_region) != 0) {
            fprintf(stderr, "Error: gr_init_shm_meta_region() returns non-zero.\n");
            exit(-1);
        }
    }
    else {
        while(1) {
            // attach to the meta-data region
            gr_shm_meta_region = df_attach_named_shm_region(gr_shm_handle, meta_region_name,
                meta_region_name_size, meta_region_size, NULL);
            if(!gr_shm_meta_region) {
                // wait until the shm base region is created
                sleep(1);
                fprintf(stderr, "Error: Cannot attach shm region. %s:%d\n",
                    __FILE__, __LINE__);
            }
            else {
                break;
            }
        }
    }
    gr_shm_meta = (gr_shm_layout_t)gr_shm_meta_region->starting_addr;
#endif

    is_simulation = 0;
    if(getenv("GR_IS_SIMULATION") != NULL) {
        is_simulation = 1;
    }
 
 #ifdef GR_HAVE_PERFCTR
    char *do_phase_perfctr_str = getenv("GR_DO_PHASE_PERFCTR");
    if(do_phase_perfctr_str != NULL) {
        int d = atoi(do_phase_perfctr_str);
        gr_do_phase_perfctr = (d == 0) ? 0:1;
    }

    if(gr_do_phase_perfctr) {
        gr_perfctr_init(gr_comm_rank);
        gr_perfctr_start(gr_comm_rank);
    }
#endif

    if(!is_simulation) { // analytics, no more work need to be done. 
        return 0;
    }

    // simulation specific initialization

    sleep(20);

    // set up config parameters from environment variables
    char *do_suspend_str = getenv("GR_DO_SUSPEND");
    if(do_suspend_str != NULL) {
        int d = atoi(do_suspend_str);
        gr_do_suspend = (d == 0) ? 0:1;
    }


    // phase performance history buffer
    // Chao: each MPI process will create a phase buffer to
    // record the length of the phase
    if(gr_create_global_phases(GR_DEFAULT_NUM_PHASES)) {
        exit(-1);
    }

    // threashold value, detemine whether the phase should be used for analysis
    char *min_phase_str = getenv("GR_MIN_PHASE_LEN");
    if(min_phase_str != NULL) {
        min_phase_length = atoi(min_phase_str);
    }

#if 0
    // Chao: gr_do_stub is going to monitor the performance of simulation 
    // to decide whether run the analysis to avoid interference. not using
    // it currently
    char *gr_do_stub_str = getenv("GR_DO_STUB");
    if(gr_do_stub_str != NULL) {
        gr_do_stub = atoi(gr_do_stub_str);
    }

    // initialize shared memory monitor buffer
    int my_local_rank = gr_get_local_rank();
    key_t mon_buffer_key = 0;

#ifdef GR_HAVE_PERFCTR
    if(gr_do_stub) {
        mon_buffer_key = my_local_rank + SHM_MONITOR_BUFFER_KEY_BASE;
        gr_mon_buffer_region = gr_create_monitor_buffer(gr_shm_handle, mon_buffer_key);  
        if(!gr_mon_buffer_region) {
            exit(-1);
        }
        gr_monitor_buffer = (gr_mon_buffer_t) gr_mon_buffer_region->starting_addr;
    }
#endif

    gr_sender_t s = &(gr_shm_meta->senders[gr_shm_meta->num_senders]);
    int num_procs = gr_get_num_procs_per_node(comm);

#ifdef GR_HAVE_PERFCTR
    s->shm_mon_buffer_key[my_local_rank] = mon_buffer_key;
#endif

    MPI_Barrier(comm);

    if(gr_get_local_rank() == 0) {
        s->app_id = gr_app_id;
        s->num_procs = num_procs;
        gr_shm_meta->num_senders ++;
    }

#ifdef GR_HAVE_PERFCTR
    if(gr_do_stub) {
        // initialize signal handling
        char *timer_str = getenv("GR_TIMER_INTERVAL");
        int timer_interval;
        if(timer_str) {
            timer_interval = atoi(timer_str);
        }
        else {
            timer_interval = GR_DEFAULT_TIMER_INTERVAL; 
        }
        timer_str = getenv("GR_MONITOR_LOCKING");
        int num_locking;
        if(timer_str) {
            num_locking = atoi(timer_str);
        }
        else {
            num_locking = GR_DEFAULT_MONITOR_LOCKING;
        }
        if(gr_stub_init(timer_interval, num_locking)) {
            exit(-1);
        }
    }
#endif
#endif

#ifdef DEBUG_TIMING
    my_rank = gr_comm_rank;
    sprintf(log_file_name, "timestamp.%d\0", my_rank);
    log_file = fopen(log_file_name, "w");
    if(!log_file) {
        fprintf(stderr, "cannot open file %s\n", log_file_name);
    }
#endif

    return 0;
}

/*
 * Finalize GoldRush runtime library.
 * Called by both simulation and analysis.
 *
 * Return 0 for success and -1 for error.
 */
int gr_finalize()
{

    df_destroy_shm_region(gr_shm_meta_region);
    df_shm_finalize(gr_shm_handle);
    gr_destroy_global_phases();
    gr_destroy_opened_files();

    if(!is_simulation) { // analytics
        gr_finalize_scheduler();
        return 0;
    }

    // simulation only

#ifdef GR_HAVE_PERFCTR
    if(gr_do_stub) {
        gr_stub_finalize();
    }
#endif

#ifdef DEBUG_TIMING
    MPI_Barrier(gr_comm);
    fprintf(log_file, "rank\tcall_count\ttotal\tmonitor\tsuspend\tschedule\tresume\tstub\n");
    fprintf(log_file, "%d\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n", 
        my_rank, call_count, total_time, monitor_time, suspend_time, schedule_time, resume_time, stub_time);
    fprintf(log_file, "phase_time:\t%lu\n", phase_time);
    fprintf(log_file, "\nTiming\n");
    gr_print_phases(log_file);
    fclose(log_file);
#endif

#ifdef GR_HAVE_PERFCTR
    if(gr_do_stub) {
        gr_destroy_monitor_buffer(gr_mon_buffer_region);
    }
    gr_perfctr_finalize(gr_comm_rank);
#endif

	return 0;
}

/* Public API used by simulation code */ 

/*
 * Mark the start of mainloop
 */
int gr_mainloop_start()
{
    is_in_mainloop = 1;
    return 0;
}

/*
 * Mark the end of mainloop
 */
int gr_mainloop_end()
{
    is_in_mainloop = 0;

    /**
     * if worker process (not leader process) yielding CPU;
     * not check the length of phase in gr_mainloop_end
     */
	if (!gr_is_main_thread()) {
        /* check whether the idle length is long enough. */
		yield_to_coop();
	}

    return 0;
}

/**
 * an wrap to gr_pahse_start using filename as an input
 */
int gr_phase_start_s(char *filename, unsigned int line) 
{
    /**
     * here using a hash function to hash the file name
     * into a unsigned long
     */
    //unsigned long int file = hash(filename);
    
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "failed to get file identifier for (%s), cannot estimate the length because of error: %s\n", filename, strerror(errno));
    } else {
        int next = gr_files->size;
        gr_files->array[next] = fd;
        gr_files->size++;

        gr_phase_start(fd, line);
    }
    
    return 0;
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
int gr_phase_start(unsigned long int file, unsigned int line)
{
//fprintf(stderr, "im here %s %lu\n", (char *)file, file);
#ifdef DEBUG_TIMING
    t1 = rdtsc();
#endif

    // estimate the length of the current phase based on history info
    // skip small phases
    gr_phase_perf_t p_perf;
    gr_phase_t p = gr_find_phase(file, line, &p_perf); // make a guess
    int should_run = 1;
    if(p && p_perf && p_perf->avg_length != 0 && p_perf->avg_length <= min_phase_length) {
        should_run = 0;
    }
    current_phase_file = file;
    current_phase_line = line;

#ifdef DEBUG_TIMING
    t2 = rdtsc();
#endif

    // resume the analysis process
    if(should_run && !gr_is_main_thread()) {
        yield_to_coop();
        is_resumed = 1;
    }

#ifdef DEBUG_TIMING
    t3 = rdtsc();
#endif

#ifdef GR_HAVE_PERFCTR
    if(gr_do_phase_perfctr) {
        gr_perfctr_read(current_phase_perfctr_values);
    }
#endif

    current_phase_start_time = rdtsc();

#ifdef DEBUG_TIMING
    t4 = rdtsc();
#endif

    if(gr_do_stub) {
        gr_stub_phase_start(current_phase_perfctr_values);
    }

#ifdef DEBUG_TIMING
    t5 = rdtsc();
#endif
    has_start_phase = 1;
    return 0;        
}

/**
 * a wraper to gr_phase_end, with parameter file as a string
 */
int gr_phase_end_s(char *filename, unsigned int line)
{
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "failed to get file identifier, cannot estimate the length \
                because of error: %s\n", strerror(errno));
    } else {
        gr_phase_end(fd, line);
    }
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
int gr_phase_end(unsigned long int file, unsigned int line)
{
  if(has_start_phase) {

#ifdef DEBUG_TIMING
    t6 = rdtsc();
#endif

    uint64_t end_cycle = rdtsc();

    long long end_perfctr_values[NUM_EVENTS];

#ifdef GR_HAVE_PERFCTR
// optimize out
    if(gr_do_phase_perfctr) {
        gr_perfctr_read(end_perfctr_values);
    }
#endif

#ifdef DEBUG_TIMING
    t7 = rdtsc();
#endif

// optimize out
    if(gr_do_stub) {
        // disable the timer
        gr_stub_phase_end();
    }

#ifdef DEBUG_TIMING
    t8 = rdtsc();
#endif

    // suspend the analysis process
    if(is_resumed) {
        // TODO: lock semaphore
        // Chao: do nothing here first, as it is main thread
#if 0
        gr_receiver_t r = gr_shm_meta->receivers;
        int num_receivers = gr_shm_meta->num_receivers;
        int i;
        for(i = 0; i < num_receivers; i ++) {
            gr_suspend_receiver(&r[i]);
        }
#endif
        is_resumed = 0;
    }

#ifdef DEBUG_TIMING
    t9 = rdtsc();
#endif

    // update phase history
    int p_index = gr_get_phase(current_phase_file, 
                               current_phase_line,
                               file, 
                               line
                              );
    if(p_index == -1) {
        return -1;
    }
    uint64_t length = end_cycle - current_phase_start_time;
#ifdef GR_HAVE_PERFCTR
// optimize out
    if(gr_do_phase_perfctr) {
        int j;
        for(j = 0; j < NUM_EVENTS; j ++) {
            end_perfctr_values[j] -= current_phase_perfctr_values[j];
        }
    }
#endif
    gr_update_phase(p_index, length, end_perfctr_values);

#ifdef DEBUG_TIMING
    t10 = rdtsc();

    if(is_in_mainloop) {
        phase_time += t6 - t5;
        total_time += (t5-t1) + (t10-t6);
        monitor_time += (t4-t3) + (t7-t6);
        stub_time += (t5-t4) + (t8-t7);
        schedule_time += t2 - t1;
        resume_time += t3 - t2;
        suspend_time += t9 - t8;
        call_count ++;
    }
#endif
    has_start_phase = 0;
    return 0;
  }
  return -1;
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
int gr_get_receivers(gr_receiver_t *receivers, int *num_receivers)
{
    // TODO: set to shm region
    *receivers = gr_shm_meta->receivers;
    *num_receivers = gr_shm_meta->num_receivers;    

#ifdef DEBUG_TIMING
    fprintf(stderr, "my_rank %d get_receivers %d\n", my_rank, gr_shm_meta->num_receivers);
#endif
    return 0;
}

/*
 * Retrieve the receiver handle by the name of the data group to which
 * the receiver is subsribing
 *
 * Parameter:
 *  data_group_name: name of the data group
 *
 * Return handle of receiver for success and NULL for error.
 */
gr_receiver_t gr_get_receiver_by_data_group(char *data_group_name)
{
    // TODO: lock semaphore
    gr_receiver_t r = gr_shm_meta->receivers;
    int num_r = gr_shm_meta->num_receivers;      
    gr_dependence_t d = gr_shm_meta->data_groups;
    int num_d = gr_shm_meta->num_files;  
    int i, j;
    for(j = 0; j < num_d; j ++) {
        if(!strcmp(d[j].data_group_name, data_group_name)) {
            for(i = 0; i < num_r; i ++) 
                if(r[i].app_id == d[j].receiver_app_id)
                    return &r[i];
        }
    }
    return NULL;
}

/*
 * Suspend a receiver's execution. All processes of the receiver
 * is suspended upon return of this function.
 * 
 * Parameter:
 *  receiver: handle to the receiver 
 *
 * Return 0 for success and -1 for error.
 */
int gr_suspend_receiver(gr_receiver_t receiver)
{
    if(gr_do_suspend) {
        int pid_index = gr_local_rank;
        while(pid_index < receiver->num_procs) {
            pid_t receiver_pid = receiver->pid[pid_index];
            pid_index += gr_local_size;   
            int rc = kill(receiver_pid, SIGSTOP);    
            if(rc != 0) {
// optimize out
//                fprintf(stderr, "Error: kill() return %d. %s:%d\n", rc, __FILE__, __LINE__);
                return -1;
            }
        }
        return 0;
// optimize out
//            pid_index += gr_local_size;   
//    if(rc == 0) {    
//        if(gr_local_rank == 0) {
//            receiver->state = GR_SUSPENDED;
//        }
    }
    else {
        return 0;
    }
}

/*
 * Resume the execution of the specified receiver. All processes of the
 * receiver is resumed upon return of this function. 
 * 
 * Parameter:
 *  receiver: handle to the receiver 
 *
 * Return 0 for success and -1 for error.
 */
int gr_resume_receiver(gr_receiver_t receiver)
{
//    if(receiver->state == GR_RUNNING) return 0;
    if(gr_do_suspend) {
        int pid_index = gr_local_rank;
        while(pid_index < receiver->num_procs) {
            pid_t receiver_pid = receiver->pid[pid_index];
            pid_index += gr_local_size;   
            int rc = kill(receiver_pid, SIGCONT);    
            if(rc != 0) {
// optimize out
//                fprintf(stderr, "Error: kill() return %d. %s:%d\n", rc, __FILE__, __LINE__);
                return -1;
            }    
        }
// optimize out
//        if(rc == 0) {
//            if(gr_local_rank == 0) {
//                receiver->state = GR_RUNNING;
//            }
//        }
        return 0;
    }
    else {
        return 0;
    }
}

/* Public API used by analysis code */

/*
 * Install a specific scheduler
 */
int gr_load_scheduler(char *sched_name, int interval)
{
    return gr_internal_load_scheduler(sched_name, interval);
}

/*
 * Register the calling program as a receiver of the data group.
 *
 * Parameter:
 *  data_group_name: name of the data group
 *  comm: MPI communicator of the calling program
 *
 * Return 0 for success and -1 for error.
 */
int gr_register_receiver(char *data_group_name, MPI_Comm comm)
{
    // TODO: lock the shm region          
    // TODO: find data_group_name
    gr_receiver_t r = &(gr_shm_meta->receivers[gr_shm_meta->num_receivers]);
    int num_procs = gr_get_num_procs_per_node(comm);
    gr_get_pids(r->pid);
    MPI_Barrier(comm);
    if(gr_get_local_rank() == 0) {  
        r->app_id = gr_app_id;
        r->num_procs = num_procs;
        r->state = GR_RUNNING;
        gr_shm_meta->num_receivers ++;
    
        // add data dependency to shm region
        gr_dependence_t d = &(gr_shm_meta->data_groups[gr_shm_meta->num_files]);
        strcpy(d->data_group_name, data_group_name);
        d->sender_app_id = gr_get_sender_app_id(data_group_name);
        d->receiver_app_id = gr_app_id;
        gr_shm_meta->num_files ++;
    } 
    return 0;
}

