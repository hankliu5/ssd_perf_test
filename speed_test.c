#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int main(int argc, const char* argv[])
{
    void *buf = NULL;
    void *in_file;
    void *out_file;
    struct timeval start, end;
    int input_fd, output_fd;
    struct stat st;
    uint64_t buf_size, file_size, remain_size, duration, offset, this_copy_size;
    char *ptr;

    if (argc < 3)
    {
        puts("usage: <in_file> <out_file>");
    }

    input_fd = open(argv[argc-2], O_RDONLY);
    output_fd = open(argv[argc-1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    fstat(input_fd, &st);
    file_size = st.st_size;

    printf("file_size: %lu\n", file_size);

    // in_file = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, input_fd, 0);

    gettimeofday(&start, NULL);
    buf = calloc(1, file_size);
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("calloc time: %lu(usec)\n", duration);

    duration = 0;
    gettimeofday(&start, NULL);
    read(input_fd, buf, file_size);
    // memcpy(buf, in_file, file_size);
    gettimeofday(&end, NULL);
    duration += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("read speed: %f\n", (double) file_size / duration);

    if (posix_fallocate(output_fd, 0, file_size))
    {
        fprintf(stderr, "Failed to create a new file\n");
        goto cleanup;
    }

    // out_file = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_fd, 0);
    gettimeofday(&start, NULL);
    write(output_fd, buf, file_size);
    // memcpy(out_file, buf, file_size);
    gettimeofday(&end, NULL);
    duration += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("write speed: %f\n", (double) file_size / duration);

    gettimeofday(&start, NULL);
    // munmap(in_file, file_size);
    // munmap(out_file, file_size);
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("munmap time: %lu(usec)\n", duration);

cleanup:
    gettimeofday(&start, NULL);
    free(buf);
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("free time: %lu(usec)\n", duration);

    gettimeofday(&start, NULL);
    close(input_fd);
    close(output_fd);
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("close time: %lu(usec)\n", duration);
    return 0;
}
