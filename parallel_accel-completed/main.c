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

/*******************************************************************************
    This is an example which showcases effectiveness of direct connection
    which helps in achieving increasing system parallelism and concurrency.
*******************************************************************************/

#include        <stdio.h>
#include        <stdlib.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <sys/time.h>
#include        <poll.h>
#include        <sys/types.h>
#include        <sys/mman.h>
#include        <stdbool.h>

#include "vadd_vmul.h"

#ifdef __SDSCC__
#include "sds_lib.h"
#else 
#define sds_alloc(x)(malloc(x))
#define sds_free(x)(free(x))
#endif

#include "sample_common.h"

// Software implementation of Vector Multiplication, the inputs 
// are of the size TEST_DATA_SIZE
void vmul_softwareGold(
                    int *in1,   //Input Matrix 1
                    int *in2,   //Input Matrix 2
                    int *out,    //Output Matrix
                    int dim
                   )
{
    //Perform vector multiply out = in1 x in2
    for(int i = 0; i < dim; i++) {
        out[i] = in1[i] * in2[i];
    }
}

// Software implementation of Vector Addition, the inputs 
// are of the size TEST_DATA_SIZE
void vadd_softwareGold(
                    int *in1,   //Input Matrix 1
                    int *in2,   //Input Matrix 2
                    int *out,    //Output Matrix
                    int dim
                   )
{
    //Perform vector addition Out = in1 + in2
    for(int i = 0; i < dim; i++) {
        out[i] = in1[i] + in2[i];
    }
}

int main(int argc, char** argv)
{
    // Size of the vector
    size_t vector_size_bytes = sizeof(int) * TEST_DATA_SIZE;
    int uio_fd;
    void* regs; 

    // Allocate buffers using sds_alloc
    // int *source_in1         = (int *) sds_alloc(vector_size_bytes);
    // int *source_in2         = (int *) sds_alloc(vector_size_bytes);
    // int *source_vadd_hw_results  = (int *) sds_alloc(vector_size_bytes);
    // int *source_vmul_hw_results  = (int *) sds_alloc(vector_size_bytes);

    struct udmabuf source_in1, source_in2, source_vadd_hw_results, source_vmul_hw_results;
    struct timeval start_time, end_time;
    if ((uio_fd = uio_open("pump-uio")) == -1) {
        printf("Can not open pump-uio\n");
        exit(1);
    }
    
    regs = mmap(NULL, vector_size_bytes, PROT_READ|PROT_WRITE, MAP_SHARED, uio_fd, 0);

    if (udmabuf_open(&source_in1, "udmabuf0") == -1)
        exit(1);
    if (udmabuf_open(&source_in2, "udmabuf1") == -1)
        exit(1);
    if (udmabuf_open(&source_vadd_hw_results, "udmabuf2") == -1)
        exit(1);
    if (udmabuf_open(&source_vmul_hw_results, "udmabuf3") == -1)
        exit(1);
    // Allocate software output buffer
    int *source_vadd_sw_results  = (int *) malloc(vector_size_bytes);
    int *source_vmul_sw_results  = (int *) malloc(vector_size_bytes);

    int size = TEST_DATA_SIZE;
    bool match = true;

    // Create the test data 
    for(int i = 0 ; i < TEST_DATA_SIZE; i++){
        ((int*)(source_in1.buf))[i] = rand();
        ((int*)(source_in2.buf))[i] = rand();
        source_vadd_sw_results[i] = 0;
        source_vmul_sw_results[i] = 0;
        ((int*)(source_vadd_hw_results.buf))[i] = 0;
        ((int*)(source_vmul_hw_results.buf))[i] = 0;
    }
    printf("pew");

    //Two hw functions are called back to back. First the 
    //vadd_accel is executed, then vmul_accel is executed.
    //The execution of both accelerators is sequential here.
    //To prevent automatic dataflow between calls to the two
    //hw functions, async and wait pragma is used here so as
    //to ensure that the two hw functions will be running sequentially.
    gettimeofday(&start_time, NULL);
    // Launch Hardware Solution
    for(int itr = 0; itr < NUM_TIMES; itr++)
    {
        #pragma SDS async(1)        
        vadd_accel(source_in1, source_in2, source_vadd_hw_results, size);
        #pragma SDS wait(1)
        #pragma SDS async(2)                        
        vmul_accel(source_in1, source_in2, source_vmul_hw_results, size);
        #pragma SDS wait(2)
    }
    gettimeofday(&end_time  , NULL);
    printf("Sequential Hardware time:");
    print_diff_time(start_time, end_time);
    printf("\n\n");

    //Two hw functions are called back to back.
    //The program running on the hardware first transfers in1 and in2 
    //to the vadd_accel hardware and returns immediately. Then the program 
    //transfers in1  and in2 to the vmul_accel hardware and returns
    //immediately. When the program later executes to the point of 
    //#pragma SDS wait(id), it waits for the particular output to be ready.
    gettimeofday(&start_time, NULL);
    // Launch Hardware Solution
	#pragma SDS async(1)
	vadd_accel(source_in1, source_in2, source_vadd_hw_results, size);
	#pragma SDS async(2)
	vmul_accel(source_in1, source_in2, source_vmul_hw_results, size);
    for(int itr = 0; itr < NUM_TIMES; itr++)
    {
       	#pragma SDS wait(1)
		#pragma SDS async(1)
       	vadd_accel(source_in1, source_in2, source_vadd_hw_results, size);
		#pragma SDS wait(2)
		#pragma SDS async(2)
      	vmul_accel(source_in1, source_in2, source_vmul_hw_results, size);
    }
    #pragma SDS wait(1)
    #pragma SDS wait(2)
    gettimeofday(&end_time  , NULL);
    printf("Parralel Hardware time:");
    print_diff_time(start_time, end_time);
    printf("\n\n");

    // Launch Software Solution
    // vadd_softwareGold(source_in1, source_in2, source_vadd_sw_results, size);
    // vmul_softwareGold(source_in1, source_in2, source_vmul_sw_results, size);

    // // Compare the results 
    // for(int i = 0 ; i < TEST_DATA_SIZE; i++){
    //     if(source_vadd_hw_results[i] != source_vadd_sw_results[i]){
    //         printf("Hardware Addition Error Mismatch");
    //         match = false;
    //         break;
    //     }
    //     if(source_vmul_hw_results[i] != source_vmul_sw_results[i]){
    //         printf("Hardware Multiplication Error Mismatch")
    //         match = false;
    //         break;
    //     }
    // }

    // Release memory
    //sds_free(source_in1);
    //sds_free(source_in2);
    //sds_free(source_vadd_hw_results);
    //sds_free(source_vmul_hw_results);
    udmabuf_close(&source_in1);
    udmabuf_close(&source_in2);
    udmabuf_close(&source_vadd_hw_results);
    udmabuf_close(&source_vmul_sw_results);
    
    free(source_vadd_sw_results);
    free(source_vmul_sw_results);

    if (match){
        printf("\n\nTest passed\n\n");
    }
    
    return (match ? 0 : 1);
}