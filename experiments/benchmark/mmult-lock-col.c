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
   mkfifo("/tmp/go-pipe", 0755);
   fp = fopen("/tmp/go-pipe", "r");
   fprintf(stderr, "> Waiting for unblock signal\n");
   fgetc(fp);
}

#define MAX_THREAD 20

int NDIM = -1;

double** a;
double** b;
double** c;

pthread_mutex_t lock;

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
      pthread_mutex_lock(&lock);
      for (j = 0; j < n; j++) {
         sum = 0.0;
         for (k = 0; k < n; k++) {
            sum = sum + a[k][i] * b[j][k];
         }
         c[j][i] = sum;
      }
      pthread_mutex_unlock(&lock);
      i += noproc;
   }
}

void* worker(void *arg) {
   parm* p = (parm*) arg;
   fprintf(stderr," p->id = %d\n", p->id);
   fprintf(stderr, "p->noproc = %d\n", p->noproc);
   fprintf(stderr, "p->dim = %d\n", p->dim);
   fprintf(stderr, "p->a = %p\n", p->a);
   fprintf(stderr, "p->b = %p\n", p->b);
   fprintf(stderr, "p->c = %p\n", p->c);
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
            e+=a[k][i]*b[j][k];

         if (e!=c[j][i]) {
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

   if (argc != 3) {
      printf("Usage: %s n dim\n  where n is no. of thread and dim is the size of matrix\n", argv[0]);
      exit(1);
   }

   n = atoi(argv[1]);

   if ((n < 1) || (n > MAX_THREAD)) {
      printf("The no of thread should between 1 and %d.\n", MAX_THREAD);
      exit(1);
   }

   NDIM = atoi(argv[2]);

   pthread_mutex_init(&lock, NULL);

   init_matrix(&a);
   init_matrix(&b);
   init_matrix(&c);

   for (i = 0; i < NDIM; i++) {
      for (j = 0; j < NDIM; j++) {
         a[j][i] = i + j;
         b[j][i] = i + j;
      }
   }

   threads = (pthread_t*) malloc(n * sizeof(pthread_t));
   pthread_attr_init(&pthread_custom_attr);

   arg = (parm*) malloc(sizeof(parm) * n);

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

   for (i = 0; i < n; i++) {
      pthread_join(threads[i], NULL);
   }

   /* print_matrix(NDIM); */
   check_matrix(NDIM);
   free(arg);

   end = get_micro_clock();
   fprintf(stderr, "> application runtime: %lld microseconds\n", end - start);

   return 0;
}

