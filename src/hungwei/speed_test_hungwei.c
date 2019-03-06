#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "pyssdnvme.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int main(int argc, const char* argv[])
{
    int i, thread_num, input_fd;
    int rc = 0;    
    struct timeval start, end;
    struct stat st;
    uint64_t duration;
    size_t file_size, ret;
    void *buf;

    if (argc < 2)
    {
        printf("usage: %s <path_to_file>\n", argv[0]);
        exit(1);
    }
    input_fd = open(argv[1], O_RDONLY);
    fstat(input_fd, &st);
    file_size = st.st_size;
    buf = calloc(1, file_size);    
    gettimeofday(&start, NULL);
    ret = pythonssd_nvme_read(input_fd, buf, file_size, 0);
    if (ret != file_size)
    {
        fprintf(stderr, "Read error\n");
        rc = 1;
        goto cleanup;
    }
    gettimeofday(&end, NULL);
    duration += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("read time: %lu us\n", duration);
cleanup:
    free(buf);
    return 0;
}
