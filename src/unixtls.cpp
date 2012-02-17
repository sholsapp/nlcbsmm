#include "VERSION.h"
#include <new>
#include <cstdio>

// For now, we only use thread-local variables (__thread)
//   (a) for Linux x86-32 platforms with gcc version > 3.3.0, and
//   (b) when compiling with the SunPro compilers.

// Compute the version of gcc we're compiling with (if any).
#define GCC_VERSION (__GNUC__ * 10000 \
      + __GNUC_MINOR__ * 100 \
      + __GNUC_PATCHLEVEL__)

#if ((GCC_VERSION >= 30300) && \
      !defined(__x86_64) && \
      !defined(__SVR4) && \
      !defined(__APPLE__)) \
|| defined(__SUNPRO_CC)
#define USE_THREAD_KEYWORD 1
#endif


#if defined(USE_THREAD_KEYWORD)

#include "realtls.cpp"

#else // !defined(USE_THREAD_KEYWORD)

#include "simtls.cpp"

#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vmmanager.h"

#if 1 // This should always be on -- just here for testing.

//
// Intercept thread creation and destruction to flush the TLABs.
//


/*
   extern "C" {

   typedef void * (*threadFunctionType) (void *);

   typedef
   int (*pthread_create_function) (pthread_t *thread,
   const pthread_attr_t *attr,
   threadFunctionType start_routine,
   void *arg);

   typedef
   void (*pthread_exit_function) (void *arg);

   }
 */

// A special routine we call on thread exits to free up some resources.
static void exitRoutine (void) {
   TheCustomHeapType * heap = getCustomHeap();

   // Clear the TLAB's buffer.
   heap->clear();

   // Relinquish the assigned heap.
   getMainHoardHeap()->releaseHeap();
}

extern "C" {

   static inline void * startMeUp (void * a)
   {
      getCustomHeap();
      getMainHoardHeap()->findUnusedHeap();
      pair<threadFunctionType, void *> * z
         = (pair<threadFunctionType, void *> *) a;

      threadFunctionType f = z->first;
      void * arg = z->second;

      void * result = NULL;
      result = (*f)(arg);
      exitRoutine();
      delete z;
      return result;
   }

}

#include <dlfcn.h>


// Intercept thread creation. We need this to first associate
// a heap with the thread and instantiate the thread-specific heap
// (TLAB).  When the thread ends, we relinquish the assigned heap and
// free up the TLAB.

#if defined(__SVR4)

extern "C" {
   typedef
      int (*thr_create_function) (void * stack_base,
            size_t stack_size,
            void * (*start_routine) (void *),
            void * arg,
            long flags,
            thread_t * new_thread_id);

   typedef
      void (*thr_exit_function) (void * arg);

}

extern "C" int thr_create (void * stack_base,
      size_t stack_size,
      void * (*start_routine) (void *),
      void * arg,
      long flags,
      thread_t * new_thread_id)
{

   // Force initialization of the TLAB before our first thread is created.
   volatile static TheCustomHeapType * t = getCustomHeap();

   char fname[] = "_thr_create";

   // Instantiate the pointer to thr_create, if it hasn't been
   // instantiated yet.

   // A pointer to the library version of thr_create.
   static thr_create_function real_thr_create =
      (thr_create_function) dlsym (RTLD_NEXT, fname);

   anyThreadCreated = 1;

   pair<threadFunctionType, void *> * args =
      //    new (_heap.malloc(sizeof(pair<threadFunctionType, void*>)))
      new
      pair<threadFunctionType, void *> (start_routine, arg);

   fprintf(stderr, ">>> thread_create...\n");

   int result = (*real_thr_create)(stack_base, stack_size, startMeUp, args, flags, new_thread_id);

   return result;
}


extern "C" void thr_exit (void *value_ptr) {

#if defined(linux) || defined(__APPLE__)
   char fname[] = "thr_exit";
#else
   char fname[] = "_thr_exit";
#endif

   // Instantiate the pointer to thr_exit, if it hasn't been
   // instantiated yet.

   // A pointer to the library version of thr_exit.
   static thr_exit_function real_thr_exit =
      (thr_exit_function) dlsym (RTLD_NEXT, fname);

   // Do necessary clean-up of the TLAB and get out.
   exitRoutine();
   (*real_thr_exit)(value_ptr);
}

#endif


extern "C" void pthread_exit (void *value_ptr) {

#if defined(linux) || defined(__APPLE__)
   char fname[] = "pthread_exit";
#else
   char fname[] = "_pthread_exit";
#endif

   // Instantiate the pointer to pthread_exit, if it hasn't been
   // instantiated yet.

   // A pointer to the library version of pthread_exit.
   static pthread_exit_function real_pthread_exit =
      reinterpret_cast<pthread_exit_function>
      (reinterpret_cast<intptr_t>(dlsym (RTLD_NEXT, fname)));

   // Do necessary clean-up of the TLAB and get out.
   exitRoutine();
   (*real_pthread_exit)(value_ptr);

   // We should not get here, but doing so disables a warning.
   exit(0);
}


extern "C" int pthread_create (pthread_t *thread,
      const pthread_attr_t *attr,
      void * (*start_routine) (void *),
      void * arg) {
   /**
    *
    */
   void*               packet_memory  = NULL;
   void*               work_memory    = NULL;
   void*               raw            = NULL;
   uint32_t            remote_ip      = 0;
   struct sockaddr_in  remote_addr    = {0};

   Packet*          p   = NULL;
   ThreadCreate*    tc  = NULL;
   ThreadCreateAck* tca = NULL;

   // Force initialization of the TLAB before our first thread is created.
   volatile static TheCustomHeapType * t = getCustomHeap();

#if defined(linux) || defined(__APPLE__)
   char fname[] = "pthread_create";
#else
   char fname[] = "_pthread_create";
#endif

   // A pointer to the library version of pthread_create.
   static pthread_create_function real_pthread_create =
      reinterpret_cast<pthread_create_function>
      (reinterpret_cast<intptr_t>(dlsym (RTLD_NEXT, fname)));

   if (!anyThreadCreated)
      anyThreadCreated = 1;

   pair<threadFunctionType, void *> * args =
      // new (_heap.malloc(sizeof(pair<threadFunctionType, void*>)))
      new
      pair<threadFunctionType, void *> (start_routine, arg);

   fprintf(stderr, ">>>> pthread_create(%s) func(%p) arg(%p)\n", local_ip, start_routine, arg);

   remote_ip = get_available_worker();

   remote_addr.sin_family      = AF_INET;
   remote_addr.sin_addr.s_addr = remote_ip;
   remote_addr.sin_port        = htons(UNICAST_PORT);

   work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
   packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

   /*
   // Push work onto the uni_speaker's queue
   safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
   // A new work tuple
   new (work_memory) WorkTupleType(remote_addr,
   // A new packet
   new (packet_memory) ThreadCreate((void*) start_routine, (void*) arg))
   );

   // Signal unicast speaker there is queued work
   cond_signal(&uni_speaker_cond);
    */

   // TODO: Fix this
   p = ClusterCoordinator::direct_comm(
         remote_ip,
         reinterpret_cast<Packet*>(
            new (packet_memory) ThreadCreate((void*) start_routine, (void*) arg))
         );

   fprintf(stderr, "> pthread got a %x back\n", p->get_flag());

   // This was returned to us, we're done with it
   clone_heap.free(tca);

   // TODO: return a valid nlcbsmm thread id (so the caller can wait for it later)
   return -1;
}

#endif
