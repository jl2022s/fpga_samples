#include        <stdio.h>
#include        <stdlib.h>
#include        <fcntl.h>
#include        <string.h>
#include        <time.h>
#include        <sys/time.h>
#include        <poll.h>
#include        <sys/types.h>
#include        <sys/mman.h>

#ifdef __SDSCC__
#include "sds_lib.h"
#else 
#define sds_alloc(x)(malloc(x))
#define sds_free(x)(free(x))
#endif

#include "sample_common.h"

#define N 16
#define NUM_ITERATIONS N

int temp;
typedef short data_t;

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

//#pragma SDS data zero_copy(a, b)

void array_zero_copy(struct udmabuf a, struct udmabuf b)
{
     temp = 0;
     for(int i = 0; i < N; i++) {
          b.buf[i] = a.buf[i];
     }
}


int compare(data_t swOut[N], struct udmabuf hwOut)
{
     for (int i = 0; i < N; i++) {
          if (swOut[i] != hwOut.buf[i]) {
               printf("Values differ!\n\n");
               return -1;
          }
     }
     printf("\n\nValues match\n\n");
     return 0;
}


void arraycopy_sw(struct udmabuf a, data_t *b)
{
     for(int i = 0; i < N; i++) {
          b[i] = a.buf[i];
     }
}

int print_results(struct udmabuf a, data_t swOut[N], struct udmabuf b){
     int i;
     printf("In ME:");
     for (i = 0; i < 0x10; i++)
     {
          printf("%u", a.buf[i]);
     }
     printf("\n\nIn SW:");
     for (i = 0; i < 0x10; i++)
     {
          printf("%i", swOut[i]);
     }     
     printf("\n\nIn HW:");
     for (i = 0; i < 0x10; i++)
     {
          printf("%u", b.buf[i]);
     }
     


}

int main(int argc, char* argv[])
{
     int            uio_fd;
     void*          regs;
     struct udmabuf A_array;
     struct udmabuf B_array;

     data_t  Bs[N];

     if ((uio_fd = uio_open("pump-uio")) == -1) {
          printf("Can not open pump-uio\n");
     }

     //data_t *A = (data_t*)sds_alloc(N * sizeof(data_t));
     //data_t *B = (data_t*)sds_alloc(N * sizeof(data_t));


     regs = mmap(NULL, N, PROT_READ|PROT_WRITE, MAP_SHARED, uio_fd, 0);

     if (udmabuf_open(&A_array, "udmabuf4") == -1)
        exit(1);
    
     if (udmabuf_open(&B_array, "udmabuf5") == -1)
        exit(1);

     int result = 0;

     for (int j = 0; j < N; j++) {
          ((unsigned char*)(A_array.buf))[j]  = j;
          ((unsigned char*)(B_array.buf))[j] = 0;
          Bs[j] = 0;
     }

     arraycopy_sw(A_array, Bs);
     array_zero_copy(A_array, B_array);
     print_results(A_array, Bs, B_array);
     result = compare(Bs, B_array);


     udmabuf_close(&A_array);
     udmabuf_close(&B_array);
     close(uio_fd);
     printf("complete");
     return result;
     }
