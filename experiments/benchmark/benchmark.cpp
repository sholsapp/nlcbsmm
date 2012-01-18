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

#include <sys/types.h>
#include <sys/stat.h>
void blocking_entry(void) {
   /**
    * Block until data read from named pipe.
    */
   FILE* fp;
   mkfifo("go-pipe", 0755);
   fp = fopen("go-pipe", "r");
   fprintf(stderr, "> Waiting for unblock signal\n");
   fgetc(fp);
}

int main(void) {

   blocking_entry();

   int*        ar;
   long         i;
   pthread_t  th1;
   subarray   sb1;

   /**
    *
    */
   fprintf(stdout, "> Initializing data...");
   ar = (int*) malloc (sizeof(int) * ARR_SZ);
   for (i = 0; i < ARR_SZ; i++) {
      ar[i] = 666;
   }
   fprintf(stdout, "done.\n");

   /**
    *
    */
   fprintf(stdout, "> Spawning pthread...");
   sb1.ar = &ar[0];
   sb1.n  = ARR_SZ;
   (void) pthread_create(&th1, NULL, incer, &sb1);
   fprintf(stdout, "done.\n");

   /**
    *
    */
   fprintf(stdout, "> Waiting for pthread...");
   (void) pthread_join(th1, NULL);
   fprintf(stdout, "done.\n");

   fprintf(stderr, "Result: ar[ARR_SZ / 2] = %d.\n", ar[ARR_SZ / 2]);

   return 0;
}
