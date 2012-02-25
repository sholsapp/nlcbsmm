#include <cstdio>
#include <cstdlib>

#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex;


typedef struct {
   int tid;
   int sleep_time;
   int count_to;
   void *location;
} info;

void* counter(void *arg) {
   fprintf(stderr, "<target-app> thread  %d worker fired!\n",((info*) arg)->tid);
   void* loc = ((info*) arg)->location;
   int sleept = ((info*) arg)->sleep_time;
   int target = ((info*) arg)->count_to;

   while(1) {
      pthread_mutex_lock (&mutex);
      if((*((int*)loc)) != target)
      {
        (*((int*)loc))++;
        printf("> %d\n",(*((int*)loc)));
      }
      else
      {
        pthread_mutex_unlock (&mutex);
        break; //Dalbey is mad right here
      }
      pthread_mutex_unlock (&mutex);
      if(sleept > 0) {
      	sleep(sleept);
      }
   }

   fprintf(stderr, "<target-app> thread %d worker done!\n", ((info*) arg)->tid);
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

int main(int argc, char *argv[]) {

   blocking_entry();
   pthread_mutex_init(&mutex, NULL);

   pthread_t  threads[100];
   info*  meta_data = NULL;
   int n;
   int sleep_t;
   int count;
   void* var_location = NULL;
   pthread_attr_t  pthread_custom_attr;

   if (argc != 4) {
      printf("Usage: %s num_threads sleep_duration count_to\n", argv[0]);
      exit(1);
   }

   n = atoi(argv[1]);
   sleep_t = atoi(argv[2]);
   count = atoi(argv[3]);

   if ((n < 1) || (sleep_t < 0) || (count < 1)) {
      printf("Bad arguments\n");
      exit(1);
   }
   pthread_attr_init(&pthread_custom_attr);

   var_location = malloc(sizeof(int));
   (*((int*)var_location)) = 0;
   fprintf(stdout, "<target-app> spawning threads...");
   for(int x=0; x<n; x++) {
       meta_data = (info*) malloc (sizeof(info));//Leaking memory, yay!
       meta_data->tid = x;
       meta_data->sleep_time = sleep_t;
       meta_data->count_to = count;
       meta_data->location = var_location;
       pthread_create(&threads[x], 
                      &pthread_custom_attr, 
                      counter, 
                      reinterpret_cast <void*> (meta_data));
   }
   fprintf(stdout, " done.\n");

   fprintf(stdout, "<target-app> waiting for pthreads...");
   for(int i=0; i<n; i++)
   {
	  pthread_join(threads[i], NULL);
   }
   fprintf(stdout, "done.\n");

   fprintf(stderr, "RESULT (addr = %p): %d.\n", var_location, *((int*)var_location));

   return 0;
}
