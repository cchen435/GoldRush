#ifndef _GR_MONITOR_BUFFER_H_
#define _GR_MONITOR_BUFFER_H_
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "gr_perfctr.h"
#include "df_shm.h"

#define SHM_MONITOR_BUFFER_KEY_BASE 1800
#define SHM_MONITOR_BUFFER_SIZE 4096

// Global variables
typedef struct _gr_monitor_buffer {
    pthread_rwlock_t rwlock;
    int phase_id;
    long long perfctr_values[NUM_EVENTS];
} gr_mon_buffer, *gr_mon_buffer_t;

df_shm_region_t gr_create_monitor_buffer(df_shm_method_t shm_handle, key_t shm_key);

int gr_destroy_monitor_buffer(df_shm_region_t region);

df_shm_region_t gr_attach_monitor_buffer(df_shm_method_t shm_handle, key_t shm_key);

#endif
