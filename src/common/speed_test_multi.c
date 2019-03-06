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
    uint64_t file_size;
    int rc;
    void *buf;
};

void* read_body(void* args)
{
    struct timeval start, end;
    int input_fd;
    int rc = 0;
    struct stat st;
    uint64_t duration, ret;
    struct thread_args *prop = (struct thread_args *) args;
#if MMAP
    void *in_file;
    void *out_file;
#endif
    input_fd = open(prop->infile, O_RDONLY);
    fstat(input_fd, &st);
    prop->file_size = st.st_size;
#if MMAP
    in_file = mmap(NULL, prop->file_size, PROT_READ, MAP_PRIVATE, input_fd, 0);
#endif
    gettimeofday(&start, NULL);
    prop->buf = calloc(1, prop->file_size);
    if (prop->buf == NULL)
    {
        fprintf(stderr, "[thread %d] cannot allocate memory\n", prop->tid);
        rc = 1;
        goto cleanup;
    }
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("calloc time: %lu(usec)\n", duration);

    duration = 0;
    gettimeofday(&start, NULL);
#if MMAP
    memcpy(prop->buf, in_file, prop->file_size);
#else
    ret = read(input_fd, prop->buf, prop->file_size);
    if (ret != prop->file_size)
    {
        fprintf(stderr, "[thread %d] read error\n", prop->tid);
        rc = 1;
        goto cleanup;
    }
#endif
    gettimeofday(&end, NULL);
    duration += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("[thread %d] read time: %lu us\n", prop->tid, duration);

    gettimeofday(&start, NULL);
#if MMAP
    munmap(in_file, prop->file_size);
#endif
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("munmap time: %lu(usec)\n", duration);

cleanup:
    gettimeofday(&start, NULL);
    close(input_fd);
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("close time: %lu(usec)\n", duration);
    return NULL;
}

void* write_body(void* args)
{
    struct timeval start, end;
    int output_fd;
    int rc = 0;
    struct stat st;
    uint64_t file_size, duration, ret;
    struct thread_args *prop = (struct thread_args *) args;
#if MMAP
    void *in_file;
    void *out_file;
#endif
    output_fd = open(prop->outfile, O_RDWR | O_CREAT | O_TRUNC, 0666);

    if (posix_fallocate(output_fd, 0, prop->file_size))
    {
        fprintf(stderr, "[thread %d] Failed to create a new file\n", prop->tid);
        rc = 1;
        goto cleanup;
    }
#if MMAP
    out_file = mmap(NULL, prop->file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_fd, 0);
#endif

    gettimeofday(&start, NULL);
#if MMAP
    memcpy(out_file, prop->buf, prop->file_size);
#else
    ret = write(output_fd, prop->buf, prop->file_size);
    if (ret != prop->file_size)
    {
        fprintf(stderr, "[thread %d] write error\n", prop->tid);
        rc = 1;
        goto cleanup;
    }
#endif
    gettimeofday(&end, NULL);
    duration += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("[thread %d] write time: %lu us\n", prop->tid, duration);

    gettimeofday(&start, NULL);
#if MMAP
    munmap(out_file, prop->file_size);
#endif
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("munmap time: %lu(usec)\n", duration);

cleanup:
    if (prop->buf != NULL)
    {
        gettimeofday(&start, NULL);
        free(prop->buf);
        gettimeofday(&end, NULL);
        duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
        prop->buf = NULL;
    }
    // printf("free time: %lu(usec)\n", duration);

    gettimeofday(&start, NULL);
    close(output_fd);
    gettimeofday(&end, NULL);
    duration = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    // printf("close time: %lu(usec)\n", duration);
    return NULL;
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

    // initialize the arguments
    for (i = 0; i < thread_num; i++)
    {
        sprintf(args[i].infile, "%s/input_%d.txt", argv[2], i);
        sprintf(args[i].outfile, "%s/output_%d.txt", argv[2], i);
        args[i].tid = i;
        args[i].rc = 0;
    }

    // measures read part
    for (i = 0; i < thread_num; i++)
    {
        rc = pthread_create(&(threads[i]), &attr, read_body, (void *)&(args[i]));
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
        rc = args[i].rc;
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


    // measures write part
    // for (i = 0; i < thread_num; i++)
    // {
    //     rc = pthread_create(&(threads[i]), &attr, write_body, (void *)&(args[i]));
    //     if (rc)
    //     {
    //         fprintf(stderr, "Failed to create thread %d\n", i);
    //         goto cleanup;
    //     }
    // }

    // thread_ret_normally = 1;
    // for (i = 0; i < thread_num; i++)
    // {
    //     rc = pthread_join(threads[i], &status);
    //     rc = args[i].rc;
    //     if (rc)
    //     {
    //         fprintf(stderr, "Failed to join thread %d\n", i);
    //     }
    //     if ((long)status != 0)
    //     {
    //         thread_ret_normally = 0;
    //         fprintf(stderr, "Failed to join thread %d\n", i);
    //         rc |= (long)status;
    //     }
    // }

    // if (thread_ret_normally == 0)
    // {
    //     goto cleanup;
    // }

    pthread_attr_destroy(&attr);
cleanup:
    free(threads);
    free(args);
    return 0;
}
