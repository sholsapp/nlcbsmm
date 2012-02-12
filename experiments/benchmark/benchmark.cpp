#include <cstdio>
#include <cstdlib>

#include <pthread.h>

#define ARR_SZ 1000000

typedef struct {
   int *ar;
   int  n;
} subarray;

void* incer(void *arg) {
   fprintf(stderr, "<target-app> thread worker fired!\n");
   fprintf(stderr, "<target-app> arg = %p (n = %d)\n", (subarray*) arg, ((subarray*) arg)->n);

   int i = 0;
   for (i = 0; i < ((subarray *)arg)->n; i++) {
      ((subarray *)arg)->ar[i]++;
   }
   fprintf(stderr, "<target-app> thread worker done!\n");
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
   int         i;
   pthread_t  th1;
   subarray   sb1;

   /**
    *
    */
   fprintf(stdout, "<target-app> initializing data...");
   ar = (int*) malloc (sizeof(int) * ARR_SZ);
   for (i = 0; i < ARR_SZ; i++) {
      ar[i] = 666;
   }
   fprintf(stdout, "done.\n");

   /**
    *
    */
   fprintf(stdout, "<target-app> spawning thread...");
   sb1.ar = &ar[0];
   sb1.n  = ARR_SZ;
   (void) pthread_create(&th1, NULL, incer, &sb1);
   fprintf(stdout, "done.\n");

   /**
    *
    */
   fprintf(stdout, "<target-app> waiting for pthread...");
   (void) pthread_join(th1, NULL);
   fprintf(stdout, "done.\n");

   fprintf(stderr, "<target-app> result: %d.\n", ar[ARR_SZ / 2]);

   return 0;
}
