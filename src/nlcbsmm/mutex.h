#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <linux/futex.h>

/**
 * A futex(2) based mutex library.
 *
 * Code adapted from http://locklessinc.com/articles/locks/
 *                   http://locklessinc.com/articles/mutex_cv_futex/
 */

typedef int mutex;

   typedef struct cv cv;
   struct cv {
      mutex *m;
      int seq;
      int pad;
   };


#define cpu_relax() asm volatile("pause\n": : :"memory")

#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1)
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))

#define MUTEX_INITIALIZER {0}

/**
 * If this many wake operations can happen between the read of the seq variable and the implementation 
 * of the FUTEX_WAIT system call, then we will have a bug. However, this is extremely unlikely due to 
 * amount of time it would take to generate that many calls.
 */
#define INT_MAX 32768

extern "C" {

   static inline unsigned xchg_32(void *ptr, unsigned x);

   int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3);

   int mutex_init(mutex *m, const pthread_mutexattr_t *a);

   int mutex_destroy(mutex *m);

   int mutex_lock(mutex *m);

   int mutex_unlock(mutex *m);

   int mutex_trylock(mutex *m);

   int cond_init(cv *c, pthread_condattr_t *a);

   int cond_destroy(cv *c);

   int cond_signal(cv *c);

   int cond_broadcast(cv *c);

   int cond_wait(cv *c, mutex *m);
}

