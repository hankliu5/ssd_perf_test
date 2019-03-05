CC=gcc
CFLAGS=-Wformat -Wunused-result -O3
CURRENT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
INCLUDES=-I/usr/include/pyssdnvme
LDFLAGS=-L$(CURRENT_DIR)/libnvmed
LIBS=-lpyssdnvme

SRCS=speed_test_hungwei.c

PROG=speed_test_hungwei

.PHONY: clean

all: benchmark

benchmark: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(PROG) $(LIBS) $(INCLUDES)

clean:
	$(RM) *.o *~ $(PROG)

