CC=gcc
CFLAGS=-Wformat -Wunused-result -O2
INCLUDES=
LDFLAGS=
LIBS=-pthread

SRCS=speed_test_multi.c

OBJS=$(SRCS:.c=.o)
PROG=speed_test_multi

.PHONY : clean
all: $(PROG)

mmap: CFLAGS+=-DMMAP
mmap: $(PROG)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	$(RM) *.o *~ $(PROG)

