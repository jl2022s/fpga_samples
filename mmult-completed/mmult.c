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

#include <stdlib.h>
#include "mmultadd.h"

#include        <stdio.h>
#include        <stdlib.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <sys/time.h>
#include        <poll.h>
#include        <sys/types.h>
#include        <sys/mman.h>


/**
 *
 * Design principles to achieve II = 1
 * 1. Stream data into local RAM for inputs (multiple access required)
 * 2. Partition local RAMs into N/2 sub-arrays for fully parallel access (dual-port read)
 * 3. Pipeline the dot-product loop, to fully unroll it
 * 4. Separate multiply-accumulate in inner loop to force two FP operators
 *
 */
struct udmabuf {
    char           name[128];
    int            file;
    unsigned char* buf;
    unsigned int   buf_size;
    unsigned long  phys_addr;
    unsigned long  debug_vma;
    unsigned long  sync_mode;
};

//void mmult (float A[N*N], float B[N*N], float C[N*N]) 
void mmult (struct udmabuf A, struct udmabuf B, float C[N*N])
{
  int            uio_fd;
  void*          regs;
  struct udmabuf temp;

  if ((uio_fd = uio_open("pump-uio")) == -1) {
        printf("Can not open pump-uio\n");
        exit(1);

  regs = mmap(NULL, N*N*sizeof(float), PROT_READ|PROT_WRITE, MAP_SHARED, uio_fd, 0);
  if (udmabuf_open(&temp, "udmabuf4") == -1)
        exit(1);
     float Abuf[N][N], Bbuf[N][N];
#pragma HLS array_partition variable=Abuf block factor=16 dim=2
#pragma HLS array_partition variable=Bbuf block factor=16 dim=1
     
     for(int i=0; i<N; i++) {
          for(int j=0; j<N; j++) {
#pragma HLS PIPELINE
               Abuf[i][j] = ((float *)(A.buf))[i * N + j];
               Bbuf[i][j] = ((float *)(B.buf))[i * N + j];
          }
     }
     
     for (int i = 0; i < N; i++) {
          for (int j = 0; j < N; j++) {
#pragma HLS PIPELINE
               float result = 0;
               for (int k = 0; k < N; k++) {
                    float term = Abuf[i][k] * Bbuf[k][j];
                    result += term;
               }
               C[i * N + j] = result;
          }
     }
     udmabuf_close(&temp);
}
}