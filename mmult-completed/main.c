/*
(c) Copyright 2013 - 2016 Xilinx, Inc. All rights reserved. 
This file contains confidential and proprietary information of Xilinx, Inc. and
is protected under U.S. and international copyright and other intellectual
property laws.
DISCLAIMER 
This disclaimer is not a license and does not grant any rights to the materials
distributed herewith. Except as otherwise provided in a valid license issued to
you by Xilinx, and to the maximum extent permitted by applicable law: (1) THESE
MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL FAULTS, AND XILINX HEREBY
DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY,
INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, OR
FITNESS FOR ANY PARTICULAR PURPOSE; and (2) Xilinx shall not be liable (whether
in contract or tort, including negligence, or under any other theory of
liability) for any loss or damage of any kind or nature related to, arising
under or in connection with these materials, including for any direct, or any
indirect, special, incidental, or consequential loss or damage (including loss
of data, profits, goodwill, or any type of loss or damage suffered as a result
of any action brought by a third party) even if such damage or loss was
reasonably foreseeable or Xilinx had been advised of the possibility of the
same.
CRITICAL APPLICATIONS
Xilinx products are not designed or intended to be fail-safe, or for use in any
application requiring fail-safe performance, such as life-support or safety
devices or systems, Class III medical devices, nuclear facilities, applications
related to the deployment of airbags, or any other applications that could lead
to death, personal injury, or severe property or environmental damage
(individually and collectively, "Critical Applications"). Customer assumes the
sole risk and liability of any use of Xilinx products in Critical Applications,
subject only to applicable laws and regulations governing limitations on product
liability.
THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE AT
ALL TIMES. 
*/
#include        <stdio.h>
#include        <stdlib.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <sys/time.h>
#include        <poll.h>
#include        <sys/types.h>
#include        <sys/mman.h>

#include "sample_common.h"


#ifdef __SDSCC__
#include "sds_lib.h"
#else 
#define sds_alloc(x)(malloc(x))
#define sds_free(x)(free(x))
#endif

#include "mmultadd.h"

#ifndef NUM_TESTS
#define NUM_TESTS 1024
#endif


static void init_arrays(struct udmabuf A,  struct udmabuf B, struct udmabuf C, struct udmabuf D, float *D_sw)
{
     for (int i = 0; i < N; i++) {
          for (int j = 0; j < N; j++) {
               ((float *)A.buf)[i * N + j] = 1+i*N+j;
               ((float *)B.buf)[i * N + j] = rand() % (N * N);
               ((float *)C.buf)[i * N + j] = i;
               ((float *)D.buf)[i * N + j] = 0.0;
               D_sw[i * N + j] = 0.0;
          }
     }
}

void mmult_golden(struct udmabuf A,  struct udmabuf B, float *C)
{
     for (int row = 0; row < N; row++) {
          for (int col = 0; col < N; col++) {
               float result = 0.0;
               for (int k = 0; k < N; k++) {
                    result += ((float *)A.buf)[row*N+k] * ((float *)B.buf)[k*N+col];
               }
               C[row*N+col] = result;
          }
     }
}

void madd_golden(float *A, struct udmabuf B, float *C)
{
     for (int row = 0; row < N; row++) {
          for (int col = 0; col < N; col++) {
               C[row*N+col] =A[row*N+col] + ((float *)B.buf)[row*N+col];
          }
     }
}

static int result_check(struct udmabuf D, float *D_sw)
{
     for (int i = 0; i < N * N; i++) {
          if (D_sw[i] != D.buf[i]) {
               printf("\n\nMismatch: data index: \n\n");
               printf("%f", i);
               printf("\ndout= ");
               printf("%f", ((float *)D.buf)[i]);
               return 1;
          }
     }
     return 0;
}

int mmult_test(struct udmabuf A,  struct udmabuf B, struct udmabuf C, struct udmabuf D, float *D_sw)
{
     struct timeval start_time, end_time;

     printf("\n\nTesting!\n\n");

     
     for (int i = 0; i < NUM_TESTS; i++) 
     {
          init_arrays(A, B, C, D, D_sw);

          float tmp[N*N], tmp1[N*N];
          gettimeofday(&start_time, NULL);
          mmult_golden(A, B, tmp);
          madd_golden(tmp, C, D_sw);
          gettimeofday(&end_time  , NULL);
          printf("Software time:");
          print_diff_time(start_time, end_time);
          printf("\n\n");

          gettimeofday(&start_time, NULL);
          mmult(A, B, tmp1);
          madd(tmp1, C, D);
          gettimeofday(&end_time  , NULL);
          printf("Hardware time:");
          print_diff_time(start_time, end_time);
          printf("\n\n");

          if (result_check(D, D_sw))
               return 1;
     }

     return 0;
}

int main(int argc, char* argv[]){
     int test_passed = 0;
     float *D_sw;
     int uio_fd;
     void* regs;
     

     int array_size = N * N * sizeof(float);

     struct udmabuf A;
     struct udmabuf B;
     struct udmabuf C;
     struct udmabuf D;

     
     if ((uio_fd = uio_open("pump-uio")) == -1) {
        printf("Can not open pump-uio\n");
        exit(1);
     }
    

    
    regs = mmap(NULL, array_size, PROT_READ|PROT_WRITE, MAP_SHARED, uio_fd, 0);

     if (udmabuf_open(&A, "udmabuf0") == -1)
        exit(1);
     if (udmabuf_open(&B, "udmabuf1") == -1)
        exit(1);
     if (udmabuf_open(&C, "udmabuf2") == -1)
        exit(1);
     if (udmabuf_open(&D, "udmabuf3") == -1)
        exit(1);

    

    // A = (float *)sds_alloc(N * N * sizeof(float));
    // B = (float *)sds_alloc(N * N * sizeof(float));
    // C = (float *)sds_alloc(N * N * sizeof(float));
    // D = (float *)sds_alloc(N * N * sizeof(float));
    D_sw = (float *)malloc(N * N * sizeof(float));


     
    //  if (!A || !B || !C || !D || !D_sw) {
    //       if (A) sds_free(A);
    //       if (B) sds_free(B);
    //       if (C) sds_free(C);
    //       if (D) sds_free(D);
    //       if (D_sw) free(D_sw);
    //       return 2;
    //  }
    //   if (!A || !B || !C || !D || !D_sw){
    //     if (A) udmabuf_close(A);
    //     if (B) udmabuf_close(B);
    //     if (C) udmabuf_close(C);
    //     if (D) udmabuf_close(D);
    //     if (D_sw) udmabuf_close(D_sw);
    //     return 2;
    //   }
    

     test_passed = mmult_test(A, B, C, D, D_sw);

     if (test_passed){
        printf("\n\nTest passed\n\n");
     }

     udmabuf_close(&A);
     udmabuf_close(&B);
     udmabuf_close(&C);
     udmabuf_close(&D);
     free(D_sw);

     return (test_passed ? -1 : 0);
}

