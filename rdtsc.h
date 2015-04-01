#ifndef __RDTSC_H_DEFINED__
#define __RDTSC_H_DEFINED__

#if 1
//added by Chao
#if 0
#include <sys/time.h>
#else 
#include <time.h>
#endif 

static __inline__ unsigned long long rdtsc(void)
{
#if 0
	unsigned long long ms;
	struct timeval curtime;
	gettimeofday(&curtime, 0);
	ms = (unsigned long long) curtime.tv_sec * 1000000 + (unsigned long long) curtime.tv_usec;
//	printf ("%lu\n", ms);
	return ms;
#else 
	unsigned long long us;
	us  = (unsigned long long)clock();
	return us;
#endif
}

#else

#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#elif defined(__powerpc__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

#else

#error "No tick counter is available!"

#endif
#endif 



/*
 * Fortran interface
 */
void init_timestamp_(int *mpi_rank);

void get_timestamp_(unsigned long long *file, unsigned long long *line, unsigned long long *tag);

void finalize_timestamp_();

/*  $RCSfile:  $   $Author: kazutomo $
 *  $Revision: 1.6 $  $Date: 2005/04/13 18:49:58 $
 */

#endif


