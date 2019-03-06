#include ../../common/make.config
CC=gcc
SRC = pyssdnvme.c fifo.c pyssdnvme.h fifo.h benchmark.c
# nvme_dev.c nvme_dev.h 
#filemap.c filemap.h

OBJS = pyssdnvme.o fifo.o 

OUTPUT = libpyssdnvme.so libpyssdnvme.a
CURRENT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CFLAGS += -O3 -fPIC -D_GNU_SOURCE -g -I/usr/local/include -I/usr/include
LD_FLAGS += -lpthread -lm 
NUM_OF_THREADS=4

all: $(OUTPUT)

benchmark: benchmark.c
	$(CC) $(CFLAGS) -DVERIFY benchmark.c -o benchmark -L$(CURRENT_DIR) -lpyssdnvme

libpyssdnvme.so: $(OBJS) 
	$(CC) $(CFLAGS) -shared -Wl,-soname,libpyssdnvme.so.1 -o libpyssdnvme.so.1 $(OBJS) -lc $(LD_FLAGS)
	ln -fs libpyssdnvme.so.1 libpyssdnvme.so

libpyssdnvme.a: $(OBJS)
	$(AR) rcs libpyssdnvme.a $(OBJS)
	
pyssdnvme.o: pyssdnvme.c pyssdnvme.h fifo.o 
	$(CC) -c -fPIC $(CFLAGS) -DNUM_OF_THREADS=$(NUM_OF_THREADS) pyssdnvme.c -o pyssdnvme.o $(LD_FLAGS)

fifo.o: fifo.c fifo.h 
	$(CC) -c $(CFLAGS) fifo.c -o fifo.o $(LD_FLAGS)

install: 
	mkdir -p /usr/local/lib
	mkdir -p /usr/include/pyssdnvme
	cp *.so /usr/local/lib
	cp *.a /usr/local/lib
	cp *.h /usr/include/pyssdnvme
		
clean: $(SRC)
	rm -f $(OUTPUT) *.so.1  *.o
