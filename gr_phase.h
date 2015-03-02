#ifndef _GR_PHASE_H_
#define _GR_PHASE_H_

#include <stdio.h>
#include <stdint.h>
#include "gr_perfctr.h"

#define GR_DEFAULT_NUM_PHASES 256
#define GR_MAX_OPEN_FILE 256

// used to store the opened file
typedef struct _gr_file_array{
    int array[GR_MAX_OPEN_FILE];
    int size; 
} gr_file_array, *gr_file_array_t;

static gr_file_array_t gr_files = NULL;

typedef struct _gr_phase {
    uint64_t start_file_no;
    uint32_t start_line_no;
    uint32_t end_line_no;
    uint64_t end_file_no;
    uint32_t count;
} gr_phase, *gr_phase_t;

typedef struct _gr_phase_perf {
    uint64_t avg_length;
    uint64_t max_length;
    uint64_t min_length;
#ifdef GR_HAVE_PERFCTR
    gr_perfctr perf_counter;
#endif
} gr_phase_perf, *gr_phase_perf_t;

int gr_create_global_phases(int max_num_phases);

void gr_destroy_global_phases();
void gr_destroy_opened_files();

/*
 * Find the phase which match the start file and line number
 */
gr_phase_t gr_find_phase(unsigned long int file, 
                         unsigned int line, 
                         gr_phase_perf_t *phase_perf
                        );

/* 
 * Precisely find a phase or allocate one if not found 
 */
int gr_get_phase(unsigned long int start_file, 
                 unsigned int start_line,
                 unsigned long int end_file,
                 unsigned int end_line 
                );

void gr_update_phase(int p_index, uint64_t length, long long *pctr_values);

void gr_print_phases(FILE *log_file);

#endif

