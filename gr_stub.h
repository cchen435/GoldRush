#ifndef _GR_STUB_H_
#define _GR_STUB_H_

#define GR_DEFAULT_TIMER_INTERVAL 1000
#define GR_DEFAULT_MONITOR_LOCKING 5

void gr_timer_handler(int signum);

int gr_stub_init(int timer_interval, int num_locking);

int gr_stub_finalize();

int gr_stub_phase_start(long long *);

int gr_stub_phase_end();

#endif
