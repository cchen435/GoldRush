#ifndef _GR_INTERNAL_H_
#define _GR_INTERNAL_H_

#include <stdint.h> 
#include <sys/types.h>
#include <semaphore.h>
#include <mpi.h>
#include "df_shm.h"
#include "goldrush.h"

#define GR_MAX_NUM_FILES 5
#define GR_MAX_NUM_RECEIVERS 5
#define GR_MAX_NUM_SENDERS 5
#define GR_MAX_NUM_PROCS 32
#define PAGE_SIZE 4096

typedef struct _gr_data_dep {
    char data_group_name[30];
    int sender_app_id;
    int receiver_app_id;
} gr_dependence, *gr_dependence_t;

typedef struct _gr_sender {
    int app_id;
    int num_procs; // per node
    key_t shm_mon_buffer_key[GR_MAX_NUM_PROCS];
} gr_sender, *gr_sender_t;

/*
 * Memory layout for shared memory meta-data region
 */
typedef struct _gr_shm_layout {
    sem_t sem;
    int num_receivers;
    int num_senders;
    int num_files;
    gr_dependence data_groups[GR_MAX_NUM_FILES];
    gr_receiver receivers[GR_MAX_NUM_RECEIVERS];
    gr_sender senders[GR_MAX_NUM_SENDERS];
} gr_shm_layout, *gr_shm_layout_t;

/*
 * Get the number of processes on each node
 */
int gr_get_num_procs_per_node(MPI_Comm comm);

/*
 * Get the pids of processes on each node.
 * This is a collective call and every process within comm communicator
 * should call this function.
 */
int gr_get_pids(pid_t *pid);

/*
 * Get application id of the sender for the specified data group.
 */
int gr_get_sender_app_id(char *data_group_name);

/*
 * Get sender info
 */
gr_sender_t gr_get_sender(int sender_app_id);

/* check whether the thread is the main thread */
int gr_is_main_thread();

/*
 * Test if the calling process is the local leader on the local node.
 * Return 1 for yes and 0 for no.
 */
int gr_is_local_leader();

/*
 * Get local rank on node
 */
int gr_get_local_rank();

/*
 * Calculate size of shared memory meta-data region
 */
int gr_get_shm_meta_region_size();

/*
 * Initialize shm meta-data region
 */
int gr_init_shm_meta_region(df_shm_region_t meta_region);

#endif
