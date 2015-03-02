#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "goldrush.h"
#include "gr_perfctr.h"
#include "gr_phase.h"

static int gr_num_phases = 0;
static int gr_max_num_phases = 0;
static gr_phase_t gr_phases = NULL;
static gr_phase_perf_t gr_phases_perf = NULL;

static gr_file_array_t gr_files = NULL;

// cache to speedup the search of gr_phases array
static int previous_phase = -1; 
static int current_phase = -1;
static int new_phase = 1;

extern int gr_do_phase_perfctr;
extern int gr_num_events;
extern int is_in_mainloop;
extern int current_phase_id;

int gr_create_global_phases(int max_num_phases)
{
    gr_files = (gr_file_array_t) calloc (GR_MAX_OPEN_FILE, sizeof(gr_file_array));
    if (!gr_files) {
        fprintf(stderr, "Error: cannot allocate memory. %s:%d\n", __FILE__, __LINE__ );
        return -1;
    }

    gr_files->size = 0;
    memset(gr_files->array, -1, sizeof( GR_MAX_OPEN_FILE *sizeof(int) ) );

    gr_phases = (gr_phase_t) calloc(GR_DEFAULT_NUM_PHASES, sizeof(gr_phase));

    if(!gr_phases) {
        fprintf(stderr, "Error: cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return -1;
    }
    if(gr_do_phase_perfctr) {
        gr_phases_perf = (gr_phase_perf_t) calloc(GR_DEFAULT_NUM_PHASES, sizeof(gr_phase_perf));
        if(!gr_phases_perf) {
            fprintf(stderr, "Error: cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
        int i;
        for(i = 0; i < gr_max_num_phases; i ++) {
#ifdef GR_HAVE_PERFCTR
            gr_perfctr_init_counter(&(gr_phases_perf[i].perf_counter));
#endif
        }
    }
    gr_max_num_phases = GR_DEFAULT_NUM_PHASES;
    gr_num_phases = 0;
    return 0;
}

void gr_destroy_global_phases()
{
    if(gr_phases) free(gr_phases);
    if(gr_phases_perf) free(gr_phases_perf);
}

void gr_destroy_opened_files()
{
    int i;
    if (!gr_files)
        return;
    for (i = 0; i < gr_files->size; i++) {
        close(gr_files->array[i]);
    }
    free(gr_files);
    return;
}

/*
 * Find the phase which match the start file and line number
 */
gr_phase_t gr_find_phase(unsigned long int file, unsigned int line, gr_phase_perf_t *phase_perf)
{
    // we can only make a guess here. we need to avoid two bad things:
    // - allow a small region with high count      
    // - skip a large region with high count
    // we use a simple scoring system to pick one: pick the most frequent one
    int i;
    float current_score = 0; 
    gr_phase_t p = gr_phases;
    new_phase = 1;
    for(i = 0; i < gr_num_phases; i ++, p ++) {
        if(file == p->start_file_no && line == p->start_line_no) {
            // a candidate
            float score = p->count;
            if(score > current_score) {
                current_phase = i;
            }
            new_phase = 0;
        }
    }
    if(new_phase) { // first time we see this phase 
        if(gr_num_phases == gr_max_num_phases) {
            gr_max_num_phases ++;
            gr_phases = (gr_phase_t) realloc(gr_phases,
                sizeof(gr_phase) * gr_max_num_phases);
            if(!gr_phases) {
                fprintf(stderr, "Error: cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
                return NULL;
            }
            gr_phases_perf = (gr_phase_perf_t) realloc(gr_phases_perf,
                sizeof(gr_phase_perf) * gr_max_num_phases);
            if(!gr_phases_perf) {
                fprintf(stderr, "Error: cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
                return NULL;
            }
        }
        gr_phases[gr_num_phases].start_file_no = file;  
        gr_phases[gr_num_phases].start_line_no = line;  
        current_phase = gr_num_phases;
    }
    *phase_perf = &(gr_phases_perf[current_phase]);
    current_phase_id = current_phase;
    return &(gr_phases[current_phase]);
} 

static inline int compare_phase(gr_phase_t p,
                                unsigned long int start_file,
                                unsigned int start_line,
                                unsigned long int end_file,
                                unsigned int end_line
                               )
{
    return ((p->start_file_no == start_file) && 
            (p->start_line_no == start_line) &&
            (p->end_file_no == end_file) &&
            (p->end_line_no == end_line));
}

/*
 * Precisely find a phase or allocate one if not found
 */
int gr_get_phase(unsigned long int start_file, 
                 unsigned int start_line,
                 unsigned long int end_file,
                 unsigned int end_line 
                )
{
    gr_phase_t p;
    gr_phase_perf_t pp;
    // we already know this is the first time we see this phase
    if(new_phase) {
        p = &(gr_phases[gr_num_phases]);  
        pp = &(gr_phases_perf[gr_num_phases]);  
        p->end_file_no = end_file;
        p->end_line_no = end_line;
        p->count = 0;
        pp->avg_length = 0;
        pp->max_length = 0;
        pp->min_length = 0;
#ifdef GR_HAVE_PERFCTR
        if(gr_do_phase_perfctr) {
            gr_perfctr_init_counter(&(pp->perf_counter));
        }
#endif
        previous_phase = gr_num_phases;
        gr_num_phases ++; 
        return previous_phase;
    }

    // test cache first
    if((current_phase != -1) && 
       compare_phase(&gr_phases[current_phase], start_file, start_line, end_file, end_line)) {
        previous_phase = current_phase;
        return previous_phase;
    } 
    
    // scan the whole gr_phases array
    // start from previous phase to exloit locality
    int i = previous_phase;
    while(i < gr_num_phases) {
        if(compare_phase(&gr_phases[i], start_file, start_line, end_file, end_line)) {
            previous_phase = i;
            return i;
        }
        i ++;
    }
    // search from beginning to previous phase
    i = 0;
    while(i < previous_phase) {
        if(compare_phase(&gr_phases[i], start_file, start_line, end_file, end_line)) {
            previous_phase = i;
            return i;
        }
        i ++;
    }
    // now we confirm it's a new phase
    if(gr_num_phases == gr_max_num_phases) {
        gr_max_num_phases ++;
        gr_phases = (gr_phase_t) realloc(gr_phases,
            sizeof(gr_phase) * gr_max_num_phases);
        if(!gr_phases) {
            fprintf(stderr, "Error: cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
    }
    p = &gr_phases[gr_num_phases];
    pp = &(gr_phases_perf[gr_num_phases]);  
    p->start_file_no = start_file;
    p->start_line_no = start_line;
    p->end_line_no = end_line;
    p->end_file_no = end_file;
    p->count = 0;
    pp->avg_length = 0;
    pp->max_length = 0;
    pp->min_length = 0;
#ifdef GR_HAVE_PERFCTR
    if(gr_do_phase_perfctr) {
        gr_perfctr_init_counter(&(pp->perf_counter));
    }
#endif
    previous_phase = gr_num_phases;
    gr_num_phases ++;
    return previous_phase;
}

void gr_update_phase(int p_index, uint64_t length, long long *pctr_values)
{
    gr_phases[p_index].count ++;
    gr_phase_perf_t pp = &gr_phases_perf[p_index];
    //pp->avg_length = (pp->avg_length * gr_phases[p_index].count + length) / gr_phases[p_index].count;
    pp->avg_length += length;
    if(length > pp->max_length) {
        pp->max_length = length;
    }
    if(pp->min_length == 0 || length < pp->min_length) {
        pp->min_length = length;
    }
#ifdef GR_HAVE_PERFCTR
// optimized out
    if(gr_do_phase_perfctr) {
        if(is_in_mainloop) 
            gr_perfctr_update(&(pp->perf_counter), pctr_values);
    }
#endif
}

void gr_print_phases(FILE *log_file)
{
    int i;
    for(i = 0; i < gr_num_phases; i ++) {
        gr_phase_t p = &gr_phases[i];
        gr_phase_perf_t pp = &gr_phases_perf[i];
        fprintf(log_file, "%d\t%llu\t%llu\t%llu\t%d\t%d\t%d\t%d\n",
            p->count,
            pp->max_length,
            pp->min_length,
            (p->count == 0)? 0 : pp->avg_length/p->count,
            p->start_file_no,
            p->start_line_no,
            p->end_file_no,
            p->end_line_no
        );
    }
#ifdef GR_HAVE_PERFCTR
    fprintf(log_file, "\nPerformance Counter\n");
    for(i = 0; i < gr_num_phases; i ++) {
        gr_phase_perf_t pp = &gr_phases_perf[i];
        gr_perfctr_print(log_file, &(pp->perf_counter), i);
    }
#endif
}

