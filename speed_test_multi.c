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

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct thread_args
{
    char infile[32];
    char outfile[32];
    uint32_t tid;
};

void* thread_body(void* args)
{
    void *buf = NULL;
    struct timeval start, end;
    int input_fd, output_fd;
    struct stat st;
    uint64_t file_size, duration, ret;
    struct thread_args *prop = (struct thread_args *) args;
#if MMAP
    void *in_file;
    void *out_file;
#endif
    input_fd = open(prop->infile, O_RDONLY);
    output_fd = open(prop->outfile, O_RDWR | O_CREAT | O_TRUNC, 0666);
    fstat(input_fd, &st);
    file_size = st.st_size;
#if MMAP
    in_file = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, input_fd, 0);
#endif
    gettimeofday(&start, NULL);
    buf = calloc(1, file_size);
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("calloc time: %lu(usec)\n", duration);

    duration = 0;
    gettimeofday(&start, NULL);
#if MMAP
    memcpy(buf, in_file, file_size);
#else
    ret = read(input_fd, buf, file_size);
#endif
    gettimeofday(&end, NULL);
    duration += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("[thread %d] read time: %lu us\n", prop->tid, duration);

    if (posix_fallocate(output_fd, 0, file_size))
    {
        fprintf(stderr, "Failed to create a new file\n");
        goto cleanup;
    }
#if MMAP
    out_file = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_fd, 0);
#endif
    gettimeofday(&start, NULL);
#if MMAP
    memcpy(out_file, buf, file_size);
#else
    ret = write(output_fd, buf, file_size);
#endif
    gettimeofday(&end, NULL);
    duration += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("[thread %d] write time: %lu us\n", prop->tid, duration);

    gettimeofday(&start, NULL);
#if MMAP
    munmap(in_file, file_size);
    munmap(out_file, file_size);
#endif
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
    return (void *)0;
}

int main(int argc, const char* argv[])
{
    int i, thread_num;
    int rc = 0;    
    pthread_t *threads;
    struct thread_args *args;
    pthread_attr_t attr;
    void *status;
    char thread_ret_normally;

    if (argc < 3)
    {
        printf("usage: %s <thread_num> <path_to_file>\n", argv[0]);
        exit(1);
    }

    thread_num = atoi(argv[1]);
    if (thread_num < 1)
    {
        printf("thread number should at least greater than 0\n");
        exit(1);
    }

    threads = calloc(thread_num, sizeof(pthread_t));
    args = calloc(thread_num, sizeof(struct thread_args));

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for (i = 0; i < thread_num; i++)
    {
        sprintf(args[i].infile, "%s/input_%d.txt", argv[2], i);
        sprintf(args[i].outfile, "%s/output_%d.txt", argv[2], i);
        args[i].tid = i;
        rc = pthread_create(&(threads[i]), &attr, thread_body, (void *)&(args[i]));
        if (rc)
        {
            fprintf(stderr, "Failed to create thread %d\n", i);
            goto cleanup;
        }
    }

    thread_ret_normally = 1;
    for (i = 0; i < thread_num; i++)
    {
        rc = pthread_join(threads[i], &status);
        if (rc)
        {
            fprintf(stderr, "Failed to join thread %d\n", i);
        }
        if ((long)status != 0)
        {
            thread_ret_normally = 0;
            fprintf(stderr, "Failed to join thread %d\n", i);
            rc |= (long)status;
        }
    }

    if (thread_ret_normally == 0)
    {
        goto cleanup;
    }
    pthread_attr_destroy(&attr);
cleanup:
    free(threads);
    free(args);
    return 0;
}
