#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

/**
 * A futex(2) based mutex library.
 *
 * Code adapted from http://locklessinc.com/articles/locks/
 *                   http://locklessinc.com/articles/mutex_cv_futex/
 */

typedef int mutex;


#define cpu_relax() asm volatile("pause\n": : :"memory")

#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1)
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))

#define MUTEX_INITIALIZER {0}

//#define PTHREAD_COND_INITIALIZER {NULL, 0, 0}

/**
 * If this many wake operations can happen between the read of the seq variable and the implementation 
 * of the FUTEX_WAIT system call, then we will have a bug. However, this is extremely unlikely due to 
 * amount of time it would take to generate that many calls.
 */
#define INT_MAX 32768

struct timespec TIMESPEC = { INT_MAX, 0 };

extern "C" {

   static inline unsigned xchg_32(void *ptr, unsigned x) {
      __asm__ __volatile__("xchgl %0,%1"
            :"=r" ((unsigned) x)
            :"m" (*(volatile unsigned *)ptr), "0" (x)
            :"memory");
      return x;
   }

   int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3) {
      return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
   }

   int mutex_init(mutex *m, const pthread_mutexattr_t *a) {
      (void) a;
      *m = 0;
      return 0;
   }

   int mutex_destroy(mutex *m) {
      // Do nothing
      (void) m;
      return 0;
   }

   int mutex_lock(mutex *m) {
      int i;
      int c;

      // Spin and try to take lock
      for (i = 0; i < 100; i++) {
         c = cmpxchg(m, 0, 1);
         if (!c) return 0;

         cpu_relax();
      }

      // The lock is now contended
      if (c == 1) c = xchg_32(m, 2);

      while (c) {
         // Wait in the kernel
         sys_futex(m, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
         c = xchg_32(m, 2);
      }

      return 0;
   }

   int mutex_unlock(mutex *m) {
      int i;

      // Unlock, and if not contended then exit
      if (*m == 2) {
         *m = 0;
      }
      else if (xchg_32(m, 0) == 1) {
         return 0;
      }

      // Spin and hope someone takes the lock
      for (i = 0; i < 200; i++) {
         if (*m) {
            // Need to set to state 2 because there may be waiters
            if (cmpxchg(m, 1, 2)) return 0;
         }
         cpu_relax();
      }

      // We need to wake someone up
      sys_futex(m, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

      return 0;
   }

   int mutex_trylock(mutex *m) {
      // Try to take the lock if it is currently unlocked
      unsigned c = cmpxchg(m, 0, 1);
      if (!c) return 0;
      return EBUSY;
   }

   typedef struct cv cv;
   struct cv {
      mutex *m;
      int seq;
      int pad;
   };


   int cond_init(cv *c, pthread_condattr_t *a) {
      (void) a;
      c->m = NULL;
      /* Sequence variable doesn't actually matter, but keep valgrind happy */
      c->seq = 0;
      return 0;
   }

   int cond_destroy(cv *c) {
      /* No need to do anything */
      (void) c;
      return 0;
   }


   int cond_signal(cv *c) {
      /* We are waking someone up */
      atomic_add(&c->seq, 1);

      /* Wake up a thread */
      sys_futex(&c->seq, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

      return 0;
   }

   int cond_broadcast(cv *c) {
      mutex *m = c->m;

      /* No mutex means that there are no waiters */
      if (!m) return 0;

      /* We are waking everyone up */
      atomic_add(&c->seq, 1);

      /* Wake one thread, and requeue the rest on the mutex */
      sys_futex(&c->seq, FUTEX_REQUEUE_PRIVATE, 1, &TIMESPEC, m, 0);

      return 0;
   }

   int cond_wait(cv *c, mutex *m) {
      int seq = c->seq;

      if (c->m != m) {
         if (c->m) return EINVAL;

         /* Atomically set mutex inside cv */
         cmpxchg(&c->m, NULL, m);

         if (c->m != m) return EINVAL;
      }

      mutex_unlock(m);

      sys_futex(&c->seq, FUTEX_WAIT_PRIVATE, seq, NULL, NULL, 0);

      while (xchg_32(m, 2)) {
         sys_futex(m, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
      }
      return 0;
   }

}
