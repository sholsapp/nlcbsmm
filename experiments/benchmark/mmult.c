/********************************************
 * Unoptimized matrix matrix multiplication *
 ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>

#include <ctime>
#include <time.h>
#include <sys/time.h>
long long int get_micro_clock() {
   timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   return (long long int) ts.tv_sec * 1000000LL + (long long int) ts.tv_nsec / 1000LL;
}
#include <sys/types.h>
#include <sys/stat.h>
void blocking_entry(void) {
   FILE* fp;
   mkfifo("go-pipe", 0755);
   fp = fopen("go-pipe", "r");
   fprintf(stderr, "> Waiting for unblock signal\n");
   fgetc(fp);
}

#define MAX_THREAD 20

int NDIM = 100;

double** a;
double** b;
double** c;

typedef struct
{
   int             id;
   int             noproc;
   int             dim;
   double  (**a),(**b),(**c);
} parm;

void init_matrix(double*** m) {
   int i;
   *m = (double**) malloc(sizeof(double*) * NDIM);
   for (i = 0; i < NDIM; i++) {
      (*m)[i] = (double*) malloc(sizeof(double) * NDIM);
   }
}

void mm(int me_no, int noproc, int n, double** a, double** b, double** c) {
   int i,j,k;
   double sum;
   i = me_no;
   while (i<n) {
      for (j = 0; j < n; j++) {
         sum = 0.0;
         for (k = 0; k < n; k++) {
            sum = sum + a[i][k] * b[k][j];
         }
         c[i][j] = sum;
      }
      i += noproc;
   }
}

void* worker(void *arg) {
   parm* p = (parm*) arg;
   mm(p->id, p->noproc, p->dim, (p->a), (p->b), (p->c));
   return NULL;
}

void print_matrix(int dim)
{
   int i,j;

   printf("The %d * %d matrix is\n", dim,dim);
   for(i=0;i<dim;i++){
      for(j=0;j<dim;j++)
         printf("%lf ",  c[i][j]);
      printf("\n");
   }
}

void check_matrix(int dim)
{
   int i,j,k;
   int error=0;

   printf("Now checking the results\n");
   for(i=0;i<dim;i++)
      for(j=0;j<dim;j++) {
         double e=0.0;

         for (k=0;k<dim;k++)
            e+=a[i][k]*b[k][j];

         if (e!=c[i][j]) {
            printf("(%d,%d) error\n",i,j);
            error++;
         }
      }
   if (error)
      printf("%d elements error\n",error);
   else
      printf("success\n");
}

int main(int argc, char *argv[]) {

   blocking_entry();

   long long int start;
   long long int end;

   start = get_micro_clock();

   int j, k, noproc, me_no;
   double sum;
   double t1, t2;

   pthread_t      *threads;
   pthread_attr_t  pthread_custom_attr;

   parm           *arg;
   int             n, i;

   init_matrix(&a);
   init_matrix(&b);
   init_matrix(&c);

   for (i = 0; i < NDIM; i++)
      for (j = 0; j < NDIM; j++)
      {
         a[i][j] = i + j;
         b[i][j] = i + j;
      }

   if (argc != 2) {
      printf("Usage: %s n dim\n  where n is no. of thread and dim is the size of matrix\n", argv[0]);
      exit(1);
   }

   n = atoi(argv[1]);

   if ((n < 1) || (n > MAX_THREAD)) {
      printf("The no of thread should between 1 and %d.\n", MAX_THREAD);
      exit(1);
   }

   NDIM = atoi(argv[2]);

   threads = (pthread_t*) malloc(n * sizeof(pthread_t));
   pthread_attr_init(&pthread_custom_attr);

   arg = (parm*) malloc(sizeof(parm) * n);
   /* setup barrier */

   /* Start up thread */

   /* Spawn thread */
   for (i = 0; i < n; i++) {
      arg[i].id = i;
      arg[i].noproc = n;
      arg[i].dim = NDIM;
      arg[i].a = a;
      arg[i].b = b;
      arg[i].c = c;
      pthread_create(&threads[i], &pthread_custom_attr, worker, (void*) (arg+i));
   }

   for (i = 0; i < n; i++)
   {
      pthread_join(threads[i], NULL);

   }
   /* print_matrix(NDIM); */
   check_matrix(NDIM);
   free(arg);

   end = get_micro_clock();
   fprintf(stderr, "> application runtime: %lld microseconds\n", end - start);

   return 0;
}


