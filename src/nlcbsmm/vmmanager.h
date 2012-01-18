/**
 * NLCBSMM Virtual Memory Manager
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>

#include <vector>


#include "sbentry.h"
#include "hoard_allocator.h"


namespace NLCBSMM {
   // The page table 
   extern std::vector<SBEntry*, HoardAllocator<SBEntry* > > metadata_vector;

   // Helper function to page align a pointer
   unsigned char* pageAlign(unsigned char* p);

   // Helper function to return page number in superblock
   unsigned int pageIndex(unsigned char* p, unsigned char* base);

   // The actual signal handler for SIGSEGV
   void signal_handler(int signo, siginfo_t* info, void* contex);

   // Registers signal handler for SIGSEGV
   void register_signal_handlers();
   void nlcbsmm_init();
   void spawn_listener_thread();

}
