/***********************************************************************************
  Implementing Breadth first search on CUDA using algorithm given in HiPC'07
  paper "Accelerating Large Graph Algorithms on the GPU using CUDA"

  Copyright (c) 2008 International Institute of Information Technology - Hyderabad. 
  All rights reserved.

  Permission to use, copy, modify and distribute this software and its documentation for 
  educational purpose is hereby granted without fee, provided that the above copyright 
  notice and this permission notice appear in all copies of this software and that you do 
  not sell the software.

  THE SOFTWARE IS PROVIDED "AS IS" AND WITHOUT WARRANTY OF ANY KIND,EXPRESS, IMPLIED OR 
  OTHERWISE.

  Created by Pawan Harish.
 ************************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "pyssdnvme.h"
//#endif

void Bench(int argc, char** argv);

////////////////////////////////////////////////////////////////////////////////
// Main Program
////////////////////////////////////////////////////////////////////////////////
int main( int argc, char** argv) 
{
	Bench( argc, argv);
}

void Usage(int argc, char**argv){

fprintf(stderr,"Usage: %s <input_file> <file_size>\n", argv[0]);

}

void Bench( int argc, char** argv) 
{

    struct timeval time_start, time_end, total_start, total_end;
    size_t filesize;
	int input_fd;
    char *input_f;
    size_t num_of_nodes;


    FILE *fp;
    int input_time,input_time_wo_init = 0;
    float *h_elements = NULL;
    float *ch_elements = NULL;
    int i;
	int fd;
	fd = open("/mnt/intel/htseng3/bench_output.nvmed.txt", O_WRONLY | O_TRUNC | O_CREAT, 0777 ^ (umask(0)));

    gettimeofday(&total_start, NULL);
    gettimeofday(&time_start, NULL);

	if(argc < 2){
	Usage(argc, argv);
	exit(0);
	}
	
	input_f = argv[1];
	input_fd = open(input_f,O_RDONLY);
	filesize = atol(argv[2]);
	num_of_nodes = atol(argv[2])/sizeof(float);

   	gettimeofday(&time_start, NULL);


	h_elements = (float*) malloc(filesize);
	gettimeofday(&time_end, NULL);
	input_time_wo_init = ((time_end.tv_sec * 1000000 + time_end.tv_usec) - (time_start.tv_sec * 1000000 + time_start.tv_usec));
	pythonssd_nvme_read(input_fd, h_elements, filesize, 0);
	gettimeofday(&time_end, NULL);
	input_time = ((time_end.tv_sec * 1000000 + time_end.tv_usec) - (time_start.tv_sec * 1000000 + time_start.tv_usec));
	fprintf(stderr,"HGProfile: FileInput %d Bandwidth %lf (%lf wo init) MB/Sec\n",input_time, (double)filesize/(input_time*1.024), (double)filesize/((input_time-input_time_wo_init)*1.024));

//	for(i=0;i<num_of_nodes;i++)
//		fprintf(stderr, "%f ", h_elements[i]);
#ifdef VERIFY	
   	gettimeofday(&time_start, NULL);
	ch_elements = (float*) malloc(filesize);
	fp = fopen(input_f,"r");
	if(!fp)
	{
		fprintf(stderr,"Error Reading graph file\n");
		return;
	}

	fread(ch_elements,sizeof(float),num_of_nodes,fp);
	fclose(fp);

	gettimeofday(&time_end, NULL);
	input_time = ((time_end.tv_sec * 1000000 + time_end.tv_usec) - (time_start.tv_sec * 1000000 + time_start.tv_usec));
	fprintf(stderr,"HGProfile: File from SSD %d Bandwidth %lf (%lf wo init) MB/Sec\n",input_time, (double)filesize/(input_time*1.024), (double)filesize/((input_time)*1.024));
    	gettimeofday(&time_start, NULL);	
	for(i=0;i<num_of_nodes;i++)
	{
		if(ch_elements[i] != h_elements[i])
		{
	        fprintf(stderr,"%d %f %f\t",i, h_elements[i],ch_elements[i]);
	        break;
		}
	}
	fprintf(stderr,"\n");
#endif
	return;
}
