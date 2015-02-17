#include <stdio.h> 
#include <stdint.h> 
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <mpi.h>
#include "gr_internal.h"

extern int gr_app_id;
extern MPI_Comm gr_comm;
extern int gr_comm_rank;
extern int gr_comm_size;
extern df_shm_method_t gr_shm_handle;
extern gr_shm_layout_t gr_shm_meta;

/* Global Variables */

/* cache a copy of this */
int gr_num_nodes = 0;

/*
 * Get the number of processes on each node
 */
int gr_get_num_procs_per_node(MPI_Comm comm)
{
    // TODO: get it from MPI runtime, e.g., OMPI_COMM_WORLD_LOCAL_SIZE 
    if(gr_num_nodes == 0) {
        char *temp_string = getenv("NUM_NODES");
        if(!temp_string) {
            fprintf(stderr, "Error: env variable NUM_NODES not set.\n");
            exit(-1);
        }
        gr_num_nodes = atoi(temp_string);
    }
    int comm_size;
    MPI_Comm_size(comm, &comm_size);
    int num_procs_per_node = comm_size / gr_num_nodes;
    return num_procs_per_node;
}

/*
 * Get the pids of processes on each node.
 * This is a collective call and every process within comm communicator
 * should call this function.
 */
int gr_get_pids(pid_t *pid)
{
    int local_rank = gr_get_local_rank();
    pid_t my_pid = getpid();
    pid[local_rank] = my_pid;
    return 0;
}

/*
 * Get application id of the sender for the specified data group.
 */
int gr_get_sender_app_id(char *data_group_name)
{
    // TODO: get this from shm transport
    return 0;
}

/*
 * Get sender info
 */
gr_sender_t gr_get_sender(int sender_app_id)
{
    int i;
    for(i = 0; i < gr_shm_meta->num_senders; i ++) {
        if(gr_shm_meta->senders[i].app_id == sender_app_id) {
            return &(gr_shm_meta->senders[i]);
        }
    }    
    return NULL;
}

/*
 * Test if the calling process is the local leader on the local node.
 * Return 1 for yes and 0 for no.
 */
int gr_is_local_leader()
{
    int local_rank = gr_get_local_rank();
//  fprintf(stderr, "rank %d local %d\n", gr_comm_rank, local_rank);
    return (local_rank == 0) && (getenv("GR_IS_SIMULATION") != NULL);
//    return (local_rank == 0);
#if 0
    int local_universe_rank = atoi(getenv("OMPI_COMM_WORLD_NODE_RANK"));
    return local_universe_rank == 0;
#endif
} 

/*
 * Get local rank on node
 */
int gr_get_local_rank() 
{
    int local_rank;
#ifdef GR_IS_TITAN
    int num_procs_per_node = gr_get_num_procs_per_node(gr_comm);
    local_rank = gr_comm_rank % num_procs_per_node;
#else
    local_rank = atoi(getenv("OMPI_COMM_WORLD_LOCAL_RANK"));
#endif
    return local_rank;
}

/*
 * Calculate size of shared memory meta-data region
 */
int gr_get_shm_meta_region_size()
{
    int region_size = sizeof(gr_shm_layout);
    if(region_size % PAGE_SIZE) {
        region_size += PAGE_SIZE - (region_size % PAGE_SIZE);
    }
    return region_size;    
}

/*
 * Initialize shm meta-data region
 */
int gr_init_shm_meta_region(df_shm_region_t meta_region)
{
    gr_shm_layout_t meta = meta_region->starting_addr;
    meta->num_files = 0;
    meta->num_receivers = 0;
    meta->num_senders = 0;
    int rc = sem_init(&meta->sem, 1, 1);
    if(rc) {
        fprintf(stderr, "Error: sem_init() returns %d.\n", __FILE__, __LINE__);
    }

    // TODO: add memory fence to avoid race condition
    return rc;
}

