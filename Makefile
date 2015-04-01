SMOKY=n
JAGUAR=n
ROHAN=n
LINUX=y
ifeq ($(JAGUAR),y)
    #use this for cray compute nodes
    CC=mpicc
    CFLAGS=-c -g -DGR_IS_TITAN=1 -DDEBUG_TIMING=1 -DGR_HAVE_PERFCTR=1 -I/fang/titan/work/pe/include -I/usr/local/include
    INSTALL_PREFIX=/fang/titan/work/pe
endif

ifeq ($(SMOKY),y)
    CC=mpicc 
    CFLAGS=-c -O2 -DGR_IS_TITAN=0 -DDEBUG_TIMING=1 -DGR_HAVE_PERFCTR=1 -I$(HOME)/work/smoky/include -I ${PAPI_ROOT}/include
 #   CFLAGS=-I$(HOME)/work/smoky/include
    INSTALL_PREFIX=$(HOME)/work/smoky
endif

ifeq ($(ROHAN),y)
    CC=mpicc -c -g -DNDEBUG=1
    INSTALL_PREFIX=$(HOME)/work/rohan
endif

ifeq ($(LINUX),y)
 #   CC=mpicc -c -g -DNDEBUG=1
    CC=mpicc -c -g -DNDEBUG=1 -DGR_HAVE_PERFCTR=1
    CFLAGS=-c -O2 -DNDEBUG=1 -DDEBUG_PHASE_LEN=1 -DDEBUG_LOGIC=1 -DDEBUG_TIMING=0 -DUSE_COOPSCHED -I$(HOME)/apps/include -L$(HOME)/apps/lib
    INSTALL_PREFIX=$(HOME)/apps
endif

OBJs=goldrush.o goldrush_f.o gr_internal.o gr_sched.o gr_perfctr.o gr_monitor_buffer.o gr_stub.o gr_phase.o

all: libgoldrush.a 

libgoldrush.a: ${OBJs}
	rm -f libgoldrush.a
	ar crvs libgoldrush.a ${OBJs}
	ranlib libgoldrush.a

gr_perf_probe: libgoldrush.a gr_perf_probe.c
	$(CC) -o gr_perf_probe gr_perf_probe.c -I. -I/fang/titan/work/pe/include -I/fang/titan/bak/papi-5.1.0/src libgoldrush.a \
        -L/fang/titan/work/pe/lib -ldf_shm -lshm_transport -L/fang/titan/bak/papi-5.1.0/src -lpapi  

.c.o :
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.o libgoldrush.a

install:
	cp goldrush.h gr_perfctr.h $(INSTALL_PREFIX)/include
	cp libgoldrush.a $(INSTALL_PREFIX)/lib


