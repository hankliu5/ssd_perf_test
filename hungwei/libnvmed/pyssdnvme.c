////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 ESCAL, NC State University.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You may
// obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0 Unless required by
// applicable law or agreed to in writing, software distributed under the
// License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for
// the specific language governing permissions and limitations under the
// License.
//
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//
//   Author: Hung-Wei Tseng
//
//   Date:   March 3 2019
//
//   Description:
//   Front-end interface of Multithreaded NVMe
//
//
////////////////////////////////////////////////////////////////////////
#include <stdio.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/time.h>

#include <math.h>

#include <string.h>
#include <stdlib.h>

#include <pthread.h>
#include <linux/fiemap.h>
#include <linux/types.h>
#include "fifo.h"
#include "pyssdnvme.h"
#define NVME_IOCTL_ID       _IO('N', 0x40)
#define NVME_IOCTL_SUBMIT_IO    _IOW('N', 0x42, struct nvme_user_io)
#define FALLOC_FL_NO_HIDE_STALE  0x4

// Defining the number of I/O threads for each process.
#ifndef NUM_OF_THREADS
#define NUM_OF_THREADS 4
#endif

// The NVMe_Chunk must be smaller than the (2^(mdts))*512 Bytes of NVMe SSD. 
#ifndef NVMED_CHUNK
// NVMED_CHUNK should be 2^(NVMe SSD's mdts+3)
#if __aarch64__
#define NVMED_CHUNK 4096
#else
#define NVMED_CHUNK 1024
#endif
#define NVMED_WRITE_CHUNK NVMED_CHUNK
#define DEVICE_MDTS NVMED_CHUNK
#endif

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, fmt,  __VA_ARGS__);
#else
#define DEBUG_PRINT(fmt, ...) ;
#endif

// Following is not open for public use
struct sector {
    unsigned long long slba;
    unsigned long long count;
};

#define FE_COUNT	8000
#define FE_FLAG_LAST	(1 <<  0)
#define FE_FLAG_UNKNOWN	(1 <<  1)
#define FE_FLAG_UNALLOC	(1 <<  2)
#define FE_FLAG_NOALIGN	(1 <<  8)

#define EXTENT_UNKNOWN (FE_FLAG_UNKNOWN | FE_FLAG_UNALLOC | FE_FLAG_NOALIGN)

struct fe_s {
	__u64 logical;
	__u64 physical;
	__u64 length;
	__u64 reserved64[2];
	__u32 flags;
	__u32 reserved32[3];
};

struct fm_s {
	__u64 start;
	__u64 length;
	__u32 flags;
	__u32 mapped_extents;
	__u32 extent_count;
	__u32 reserved;
};

struct fs_s {
	struct fm_s fm;
	struct fe_s fe[FE_COUNT];
};

#define FIEMAP	_IOWR('f', 11, struct fm_s)
#define SECTOR_OFFSET 9

struct nvme_user_io {
    __u8    opcode;
    __u8    flags;
    __u16   control;
    __u16   nblocks;
    __u16   rsvd;
    __u64   metadata;
    __u64   addr;
    __u64   slba;
    __u32   dsmgmt;
    __u32   reftag;
    __u16   apptag;
    __u16   appmask;
};


// NVMe tasks for multithreaded I/O
struct nvme_task
{
    int devfd;
    int fd;
    void *host_memory_address;
    int status;
    void* (*func)(void *arg); // callback func
    void* args;
    int number_of_remaining_requests;
    struct fifo *ssd_host_queue;
    struct nvme_request* requests;
    int threaded;
};

// Thread function prototype
void *nvme_read_thread(void *t);

static int find_dev(dev_t dev)
{
    static struct dev_cache {
        dev_t dev;
        int fd;
    } cache[] = {[16] = {.dev=-1}};

    for (struct dev_cache *c = cache; c->dev != -1; c++)
        if (c->dev == dev)
            return c->fd;

    const char *dir = "/dev";
    DIR *dp = opendir(dir);
    if (!dp)
        return -errno;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_type != DT_UNKNOWN  && entry->d_type != DT_BLK)
            continue;

        struct stat64 st;
        if (fstatat64(dirfd(dp), entry->d_name, &st, 0))
            continue;

        if (!S_ISBLK(st.st_mode))
            continue;

        if (st.st_rdev != dev)
            continue;


        int ret = openat(dirfd(dp), entry->d_name, O_RDONLY);

        for (struct dev_cache *c = cache; c->dev != -1; c++) {
            if (c->dev == 0) {
                c->dev = dev;
                c->fd = ret;
            }
        }

        closedir(dp);
        return ret;
    }

    errno = ENOENT;
    closedir(dp);
    return -1;
}

int nvme_dev_find(dev_t dev)
{
    int devfd = find_dev(dev);
    if (devfd < 0)
        return -EPERM;

    if (ioctl(devfd, NVME_IOCTL_ID, 0) < 0)
        return -ENXIO;

    return devfd;
}


int nvme_dev_read(int devfd, int slba, int nblocks, void *dest)
{
    char meta[nblocks*16];

    struct nvme_user_io iocmd = {
        .opcode = nvme_cmd_read,
        .slba = slba,
        .nblocks = nblocks-1,
        .addr = (__u64) dest,
        .metadata = (__u64)0,
        .control = 0,
    };
//        .metadata = (__u64) meta,

    return ioctl(devfd, NVME_IOCTL_SUBMIT_IO, &iocmd);
}


struct nvme_request
{
    int devfd;
    void *host_memory_address;
    unsigned long slba;
    unsigned long count;
    unsigned long size;
};

struct nvme_thread_parameter
{
    struct fifo *nvme_incoming_requests_fifo;
    int *number_of_remaining_requests;
    void *task;
};
#if __aarch64__
inline void atomic_add(int i, volatile int *pw)
{
	unsigned long tmp;
	int result;

	asm volatile("// atomic_add\n"
"1:	ldxr	%w0, [%3]\n"
"	add	%w0, %w0, %w4\n"
"	stxr	%w1, %w0, [%3]\n"
"	cbnz	%w1,1b"
	: "=&r" (result), "=&r" (tmp), "+o" (*pw)
	: "r" (pw), "Ir" (i)
	: "cc");
}

inline void atomic_sub(int i, volatile int *pw)
{
	unsigned long tmp;
	int result;

	asm volatile("// atomic_sub\n"
"1:	ldxr	%w0, [%3]\n"
"	sub	%w0, %w0, %w4\n"
"	stxr	%w1, %w0, [%3]\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+o" (*pw)
	: "r" (pw), "Ir" (i)
	: "cc");
}
#endif

inline void atomic_increment(volatile int *pw)
{
#if __aarch64__
    atomic_add(1, pw);
#else
    __asm (
        "lock\n\t"
        "incl %0":
        "=m"(*pw): // output (%0)
        "m"(*pw): // input (%1)
        "cc" // clobbers
        );
#endif
}

inline void atomic_decrement(volatile int *pw)
{
#if __aarch64__
    atomic_sub(1, pw);
#else
    __asm (
         "lock\n\t"
         "decl %0":
         "=m"(*pw): // output (%0)
         "m"(*pw): // input (%1)
         "cc" // clobbers
         );
#endif
}

inline int submit_request(struct fifo *queue, struct nvme_request *request, int devfd, size_t slba, size_t count, size_t size, void *host_memory_address)
{
    request->devfd = devfd;
    request->host_memory_address = host_memory_address;
    request->slba = slba;
    request->count = count;
    request->size = size;
    fifo_push(queue, request);
}

static int generate_requests(int fd, struct stat64 *st, size_t offset, size_t length, struct fifo *fifo, struct nvme_request *request, void *host_memory_address, int *number_of_requests)
{
    int blk_size = st->st_blksize >> SECTOR_OFFSET;
    size_t processed_size = 0;
//    fprintf(stderr, "offset: 0x%x length: %llu\n", offset, length);
    if(length == 0)
    {
        length = st->st_size;
        length-=offset;
    }
    int devfd = nvme_dev_find(st->st_dev);
    if (devfd < 0) {
        if (devfd == -ENXIO)
        {
            fprintf(stderr, "NVMeD does not work on nvme device.\n");
        }
        else
        {
            fprintf(stderr, "NVMeD does not have device permission.\n");
        }
        return -1;
    }
    unsigned long num_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize;
    int i, err;
    int list_count = 1;
    size_t slba = 0, count =0;
#ifdef DEBUG
    struct timeval time_start, time_end;
            gettimeofday(&time_start, NULL);    
#endif
    struct fs_s fs;
    //Seems we can't transfer more than 65536 LBAs (2^16) at once so
    // in that case we split it into multiple transfers
    int chunk_offset = (int)log2(NVMED_CHUNK);

    memset(&fs, 0, sizeof(fs));
    fs.fm.length = length;
    fs.fm.flags  = 0;
    fs.fm.start = offset;
    fs.fm.extent_count = FE_COUNT;
    #ifdef DEBUG
    fprintf(stderr, "offset: 0x%x length: %llu\n", offset, length);
    #endif
    __u64 chunk_num, length_of_the_extent, copied_length_of_the_extent;
//    if (offset != 0 && (!(err = ioctl(fd, FIEMAP, &fs))))
    if (!(err = ioctl(fd, FIEMAP, &fs)))
    {
        #ifdef DEBUG
        fprintf(stderr, "FIEMAP %d\n",fs.fm.mapped_extents);
        #endif
        for (int j = 0; j < fs.fm.mapped_extents; j++) 
        {
            copied_length_of_the_extent = 0;
            length_of_the_extent = fs.fe[j].length;
            slba = fs.fe[j].physical >> SECTOR_OFFSET;
            #ifdef DEBUG
            fprintf(stderr, "log=%p phy=%p len=%llu flags=0x%x\n", fs.fe[j].logical,
                                fs.fe[j].physical, fs.fe[j].length, fs.fe[j].flags);
            #endif
            if(j == 0 && offset != 0)
            {
                length_of_the_extent -= (offset-fs.fe[j].logical);
                if(list_count == 1) // The first sector, starting offset is within the extend.
                {
                        slba = (fs.fe[j].physical + (offset - fs.fe[j].logical)) >> SECTOR_OFFSET;
                }
            }
            chunk_num = slba >> chunk_offset;
                while(copied_length_of_the_extent < length_of_the_extent)
                {
                    count = (((++chunk_num) << chunk_offset) - slba);
//                    if(copied_length_of_the_extent + (count << SECTOR_OFFSET) >= length_of_the_extent)
                    if(copied_length_of_the_extent + (count << SECTOR_OFFSET) >= length_of_the_extent)
                    {
                        count = (length_of_the_extent - copied_length_of_the_extent) >> SECTOR_OFFSET;
                        copied_length_of_the_extent += (count << SECTOR_OFFSET);
                        processed_size += (count << SECTOR_OFFSET);
//                        #ifdef DEBUG
//                        fprintf(stderr, "FIEMAP start: %p length: %llu copied %llu extent_length %llu %d\n", slba, count << SECTOR_OFFSET, copied_length_of_the_extent, length_of_the_extent, list_count);
//                        #endif
                        if(processed_size >= length)
                            count -= ((processed_size - length) >> SECTOR_OFFSET);
                        submit_request(fifo, request, devfd, slba, count, count << SECTOR_OFFSET, host_memory_address);
                        request++;
                        atomic_increment(number_of_requests);
                        if(host_memory_address != NULL)
                            host_memory_address+=(count << SECTOR_OFFSET);
                        if(processed_size >= length)
                            return list_count;
                        list_count++;
                        break;
                        // The very last piece in the extent!
                    }
                    processed_size += (count << SECTOR_OFFSET);
                        if(processed_size >= length)
                            count -= ((processed_size - length) >> SECTOR_OFFSET);

                    submit_request(fifo, request, devfd, slba, count, count << SECTOR_OFFSET, host_memory_address);
                    request++;
                    atomic_increment(number_of_requests);
                    if(host_memory_address != NULL)
                        host_memory_address+=(count << SECTOR_OFFSET);
                    copied_length_of_the_extent += (count << SECTOR_OFFSET);
//                    #ifdef DEBUG
//                    fprintf(stderr, "FIEMAP start: %p length: %llu copied %llu extent_length %llu %d\n", slba, count << SECTOR_OFFSET, copied_length_of_the_extent, length_of_the_extent, list_count);
//                    #endif
                    if(processed_size >= length)
                        return (list_count);
                    slba = chunk_num << chunk_offset;
                    list_count++;
                }
            }
            #ifdef DEBUG
            fprintf(stderr, "block number: %d\n",i);
            #endif
    }
    else
    {
//        fprintf(stderr, "FIEMAP error %d, trying FIBMAP\n", err);
        for (i = 0; i < num_blocks; i++) {
            unsigned long blknum = i;

            if (ioctl(fd, FIBMAP, &blknum) < 0)
            {
                return -1;
            }

            //Seems we can't transfer more than 65536 LBAs at once so
            // in that case we split it into multiple transfers
            if (i != 0 && blknum * blk_size == slba + count &&
                count + blk_size <= NVMED_CHUNK) {
                count += blk_size;
                continue;
            }
            

            if (i != 0) {
                if(offset > 0)
                {
                    if((offset >> SECTOR_OFFSET) < count) // starting address falls within this block
                    {
                        slba += (offset >> SECTOR_OFFSET);
                        count -= (offset >> SECTOR_OFFSET);
                        offset = 0;
                    }
                    else
                    {
                        offset -= (count << SECTOR_OFFSET);
                        slba = blknum * blk_size;
                        count = blk_size;
                        continue;
                    }
                }
                processed_size += (count << SECTOR_OFFSET);
                if(processed_size >= length)
                {
                    count -=((processed_size-length) >> SECTOR_OFFSET);
                    #ifdef DEBUG
                    fprintf(stderr, "FIBMAP (last) start: %p length: %llu copied: %llu total length: %llu\n", slba, count << SECTOR_OFFSET, processed_size, length);
                    #endif
//                    list_count++;
                    submit_request(fifo, request, devfd, slba, count, count << SECTOR_OFFSET, host_memory_address);
                    request++;
                    atomic_increment(number_of_requests);
                    if(host_memory_address != NULL)
                        host_memory_address+=(count << SECTOR_OFFSET);
                    copied_length_of_the_extent += (count << SECTOR_OFFSET);
                    return list_count;
                }
                submit_request(fifo, request, devfd, slba, count, count << SECTOR_OFFSET, host_memory_address);
                #ifdef DEBUG
                fprintf(stderr, "FIBMAP start: %p length: %llu copied: %llu total length: %llu\n", slba, count << SECTOR_OFFSET, processed_size, length);
                #endif
                request++;
                atomic_increment(number_of_requests);
                if(host_memory_address != NULL)
                    host_memory_address+=(count << SECTOR_OFFSET);
                copied_length_of_the_extent += (count << SECTOR_OFFSET);

                list_count++;
            }
            slba = blknum * blk_size;
            count = blk_size;
        }
        if(processed_size < length)
        {
            if(processed_size + (count << SECTOR_OFFSET) >= length)
            count -=((processed_size + (count << SECTOR_OFFSET) - length) >> SECTOR_OFFSET);
            submit_request(fifo, request, devfd, slba, count, count << SECTOR_OFFSET, host_memory_address);
//            request++;
            atomic_increment(number_of_requests);
//          fprintf(stderr, "FIBMAP start: %p length: %llu copied: %llu total length: %llu\n", slist->slba, slist->count << SECTOR_OFFSET, processed_size, length);
//          list_count++;
//            return list_count;
        }
    }
        #ifdef DEBUG
        gettimeofday(&time_end, NULL);
        printf("HGProfile: FIEMAP %ld\n",((time_end.tv_sec * 1000000 + time_end.tv_usec) - (time_start.tv_sec * 1000000 + time_start.tv_usec)));
        gettimeofday(&time_start, NULL);    
        #endif
    return list_count;
}


size_t pythonssd_nvme_read(int fd, void *memPtr, size_t size, long file_offset)
{
    int map_error;
    struct stat64 st;
    size_t length;
    
    struct nvme_request *requests;
    int i;
    pthread_t threads[NUM_OF_THREADS];
    struct fifo *nvme_requests_fifo;
    struct nvme_thread_parameter thread_info;
    int number_of_remaining_requests;
    void *current;
    struct sector current_sector;

#ifdef DEBUG
    struct timeval time_start, time_end;
    gettimeofday(&time_start, NULL);	
#endif

    if (fstat64(fd, &st))
        return 0;

    int devfd = nvme_dev_find(st.st_dev);
    if (devfd < 0) {
        fprintf(stderr,"Error");
        return 0;
    }

    if(size == 0)
        length = st.st_size;
    else
        length = size;

    if(file_offset > 0 && size == 0)
        length -= file_offset;

    unsigned long num_blocks = (st.st_size + st.st_blksize - 1) / st.st_blksize;
    requests = (struct nvme_request *)calloc(num_blocks, sizeof(struct nvme_request));
    number_of_remaining_requests = 1;
    int number_of_fifo_entries = (int)pow(2,ceil(log2(num_blocks*65536/NVMED_CHUNK)));
    nvme_requests_fifo = fifo_new(number_of_fifo_entries);
    thread_info.number_of_remaining_requests = &number_of_remaining_requests;
    thread_info.nvme_incoming_requests_fifo = nvme_requests_fifo;
    current = memPtr;

    for(int i = 0; i < NUM_OF_THREADS; i++)
    {
            pthread_create(&threads[i], NULL, nvme_read_thread, &thread_info);
    }
#ifdef DEBUG
    fprintf(stderr,"nvme_send(%p,%llu,%llu)\n",current,length,file_offset);
#endif
    memset(&current_sector, 0, sizeof(struct sector));

    int sector_count = generate_requests(fd, &st, file_offset, length, nvme_requests_fifo, requests, memPtr, &number_of_remaining_requests);
    atomic_decrement(&number_of_remaining_requests);
    
    if (sector_count < 0) {
        goto free_and_fallback;
    }
    for(int i = 0; i < NUM_OF_THREADS; i++)
        pthread_join(threads[i], NULL);

    fifo_free(nvme_requests_fifo);
    free(requests);
    return size;

free_and_fallback:
    free(requests);

fallback:
    return 0;
}


void *nvme_read_thread(void *t)
{
    struct nvme_request *request;
    struct nvme_thread_parameter *thread_info = (struct nvme_thread_parameter *)t;
    struct fifo *nvme_requests_fifo = thread_info->nvme_incoming_requests_fifo;
    volatile int *number_of_remaining_requests = thread_info->number_of_remaining_requests;
    unsigned long offset = 0;
#ifdef DEBUG
    struct timeval time_start, time_end;
#endif
    while(*number_of_remaining_requests > 0)
    {
        if((request = (struct nvme_request *)fifo_pop(nvme_requests_fifo))!=NULL)
        {
            #ifdef DEBUG
            gettimeofday(&time_start, NULL);	
            #endif

            if (nvme_dev_read(request->devfd, request->slba, request->count, request->host_memory_address))
            {
                fprintf(stderr,"nvmed read error\n");
                return NULL;
            }
//            fprintf(stderr,"nvmed read: %d %lx %lu %p\n",request->devfd, request->slba, request->count, request->host_memory_address);
            atomic_decrement(number_of_remaining_requests);
            #ifdef DEBUG
            gettimeofday(&time_end, NULL);
            printf("HGProfile: threaded read %d\n",((time_end.tv_sec * 1000000 + time_end.tv_usec) - (time_start.tv_sec * 1000000 + time_start.tv_usec)));
            #endif
        }
    }
    fifo_close(nvme_requests_fifo);
    return NULL;
}
