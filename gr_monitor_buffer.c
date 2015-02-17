#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "df_shm.h"
#include "gr_perfctr.h"
#include "gr_monitor_buffer.h"

df_shm_region_t gr_create_monitor_buffer(df_shm_method_t shm_handle, key_t shm_key)
{
    // create a shared memory region
    void *shm_region_name = (void *) &shm_key;
    int shm_region_name_size = sizeof(shm_key);     
    df_shm_region_t buffer_region = df_create_named_shm_region(shm_handle,
        shm_region_name, shm_region_name_size, SHM_MONITOR_BUFFER_SIZE, NULL);
    if(!buffer_region) {
        fprintf(stderr, "Error: Cannot create shared memory for monitor buffer. %s:%d\n",
            __FILE__, __LINE__);
        return NULL;
    }
    gr_mon_buffer_t mon_buffer = (gr_mon_buffer_t) buffer_region->starting_addr;

    // set up a read-write lock in shared memory
    pthread_rwlockattr_t rwlattr;
    if(pthread_rwlockattr_init(&rwlattr)) {
        fprintf(stderr, "Error: pthread_rwlockattr_init() returns %d. %s:%d\n",
            __FILE__, __LINE__);
        return NULL;
    }
    if(pthread_rwlockattr_setpshared(&rwlattr, PTHREAD_PROCESS_SHARED)) {
        fprintf(stderr, "Error: pthread_rwlockattr_setpshared() returns %d. %s:%d\n",
            __FILE__, __LINE__);
        return NULL;
    }
    if(pthread_rwlock_init(&(mon_buffer->rwlock), &rwlattr)) {
        fprintf(stderr, "Error: pthread_rwlock_init() returns %d. %s:%d\n",
            __FILE__, __LINE__);
        return NULL;
    }
    // pthread_rwlockattr_destroy(&rwlattr);
  
    // set up a monitor buffer in shared memory
    memset(mon_buffer->perfctr_values, 0, sizeof(long long)*NUM_EVENTS);
    return buffer_region;
}

int gr_destroy_monitor_buffer(df_shm_region_t region)
{
    gr_mon_buffer_t mon_buffer = (gr_mon_buffer_t) region->starting_addr;

    // release the lock
    // TODO: use reference count on monitor buffer
    //pthread_rwlock_destroy(&(mon_buffer->rwlock));

    // detach the shared memory buffer
    df_destroy_shm_region(region);
    return 0;
}

df_shm_region_t gr_attach_monitor_buffer(df_shm_method_t shm_handle, key_t shm_key)
{
    df_shm_region_t mon_buffer_region = NULL;   
    void *shm_region_name = (void *) &shm_key;
    int shm_region_name_size = sizeof(shm_key);

    while(1) {
        // attach to the shared memory region
        mon_buffer_region = df_attach_named_shm_region(shm_handle, shm_region_name,
            shm_region_name_size, SHM_MONITOR_BUFFER_SIZE, NULL);
        if(!mon_buffer_region) {
            // wait until the shm region is created
            sleep(1);
            fprintf(stderr, "Error: Cannot attach shm region. %s:%d\n",
                __FILE__, __LINE__);
        }
        else {
            break;
        }
    }
    return mon_buffer_region;
}

