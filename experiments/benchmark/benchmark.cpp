#include <cstdio>
#include <cstdlib>

#include <pthread.h>

#define ARR_SZ 1000000

typedef struct {
   int *ar;
   long  n;
} subarray;

void* incer(void *arg) {
   long i = 0;
   for (i = 0; i < ((subarray *)arg)->n; i++) {
      ((subarray *)arg)->ar[i]++;
   }
}


int main(void) {
   int        ar[ARR_SZ];
   long         i;
   pthread_t  th1;
   subarray   sb1;

   fprintf(stdout, "> Initializing data...");
   for (i = 0; i < ARR_SZ; i++) {
     ar[i] = 666;
   }
   fprintf(stdout, "done.\n");

   fprintf(stdout, "> Spawning pthread...");
   sb1.ar = &ar[0];
   sb1.n  = 500000;
   (void) pthread_create(&th1, NULL, incer, &sb1);
   fprintf(stdout, "done.\n");

   fprintf(stdout, "> Waiting for pthread...");
   (void) pthread_join(th1, NULL);
   fprintf(stdout, "done.\n");

   fprintf(stderr, "Result: ar[666] = %d.\n", ar[666]);

   return 0;
}
