/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

/******************************************************************************
    This is a simple vector addition based example which showcases loop 
    pipelining feature and explain about sds pragmas and APIs of SDx tool chain.
******************************************************************************/

#include        <stdlib.h>
#include        <stdbool.h>
#include        <stdio.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <sys/time.h>
#include        <poll.h>
#include        <sys/types.h>
#include        <sys/mman.h>
#include        "vector_addition.h"

#ifdef __SDSCC__
#include "sds_lib.h"
#else 
#define sds_alloc(x)(malloc(x))
#define sds_free(x)(free(x))
#endif



#ifndef NUM_TIMES
#define NUM_TIMES 2  
#endif

#include "sample_common.h"

void vadd_accel(struct udmabuf a, struct udmabuf b, struct udmabuf c, const int len);


// Compare software and hardware solutions
bool verify(int *gold, struct udmabuf out, int size) {
    for(int i = 0; i < size; i++){
        if(gold[i] != ((int *)(out.buf))[i]){
                printf("\n\nMismatch: data\n\n");
            return false;
        }
  }
  return true;
}
int uio_irq_on(int uio_fd)
{
    unsigned int  irq_on = 1;
    write(uio_fd, &irq_on, sizeof(irq_on));
}

int uio_wait_irq(int uio_fd)
{
    unsigned int  count = 0;
    return read(uio_fd, &count,  sizeof(count));
}

int main(int argc, char** argv)
{

    bool test_passed;
    int test_size = TEST_DATA_SIZE;
    int uio_fd;
    void* regs;
    int array_size = test_size * sizeof(int);
    
    srand(time(NULL));
     

    //Create buffers using sds_alloc(). sds_alloc allocate a physical contiguous memory. 
    //Contiguous memory allocations are needed if hardware function directly would like to 
    //access DDR (zero_copy pragma) or when Simple DMA is required for data transfer.
    
    //int *a = (int *) sds_alloc(sizeof(int) * test_size);
    //int *b = (int *) sds_alloc(sizeof(int) * test_size);
    //int *hw_results = (int *) sds_alloc(sizeof(int) * test_size);

    struct udmabuf a, b , hw_results;

    if ((uio_fd = uio_open("pump-uio")) == -1) {
        printf("Can not open pump-uio\n");
        exit(1);
    }
    
    regs = mmap(NULL, array_size, PROT_READ|PROT_WRITE, MAP_SHARED, uio_fd, 0);

    if (udmabuf_open(&a, "udmabuf0") == -1)
        exit(1);
    if (udmabuf_open(&b, "udmabuf1") == -1)
        exit(1);
    if (udmabuf_open(&hw_results, "udmabuf2") == -1)
        exit(1);


    // Software output buffer
    int *gold = (int *)malloc(sizeof(int) * test_size);

    // Check for failed memory allocation

    struct timeval start_time, end_time;

    for (int i = 0; i < NUM_TIMES; i++)
    {
        //Creating Test Data and golden data
        for (int i = 0 ; i < test_size ; i++){
            ((int *)(a.buf))[i] = rand();  
            ((int *)(b.buf))[i] = rand();
            // Calculate Golden value
            gold[i] = ((int *)(a.buf))[i] + ((int *)(b.buf))[i]; 
            ((int*)(hw_results.buf))[i] = 0 ;     
        }
    
        gettimeofday(&start_time, NULL);
        //Launch the Hardware Solution
        vadd_accel(a, b, hw_results, test_size);
        gettimeofday(&end_time  , NULL);
        printf("Hardware time:");
        print_diff_time(start_time, end_time);
        printf("\n\n");
    
        test_passed = verify(gold, hw_results, test_size);
    }


    //free the allocated memory
    
    //sds_free(a);
    //sds_free(b);
    //sds_free(hw_results);

    udmabuf_close(&a);
    udmabuf_close(&b);
    udmabuf_close(&hw_results);
    
    free(gold);

    if (test_passed){
        printf("\n\nTest Passsed\n\n");
    }

    return (test_passed ? 0 : -1);
}