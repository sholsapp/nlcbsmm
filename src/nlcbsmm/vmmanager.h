/**
 * NLCBSMM Virtual Memory Manager
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>

#include "sbentry.h"

namespace NLCBSMM {
   /*
    * Global Data Section
    */

   extern LinkedList metadata;

   /**
    * Helper function to page align a pointer
    */
   unsigned char* pageAlign(unsigned char* p);

   /**
    * The actual signal handler for SIGSEGV
    */
   void signal_handler(int signo, siginfo_t* info, void* contex);

   /**
    * Registers signal handler for SIGSEGV
    */
   void register_signal_handlers();

   void nlcbsmm_init();

   void spawn_listener_thread();


}



