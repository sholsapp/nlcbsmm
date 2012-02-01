/**
 * NLCBSMM Virtual Memory Manager
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>
#include <vector>

#include "page.h"
#include "cln_allocator.h"
#include "pt_allocator.h"

namespace NLCBSMM {

   // A vector of NLCBSMM::Page objects
   typedef std::vector<Page*,
           PageTableAllocator<Page*> > PageVectorType;

   // A mapping of 'ip address' to a NLCBSMM::PageVector
   typedef std::map<const char*,
           PageVectorType*,
           std::less<const char*>,
           PageTableAllocator<std::pair<const char*, PageVectorType*> > > PageTableType;
}

namespace NLCBSMM {
   // The node's IP address
   extern const char* local_ip;

   // The page table
   //extern std::vector<SBEntry*, HoardAllocator<SBEntry* > > metadata_vector;

   // Helper function to page align a pointer
   unsigned char* pageAlign(unsigned char* p);

   // Helper function to return page number in superblock
   unsigned int pageIndex(unsigned char* p, unsigned char* base);

   // The actual signal handler for SIGSEGV
   void signal_handler(int signo, siginfo_t* info, void* contex);

   // Registers signal handler for SIGSEGV
   void register_signal_handlers();
   void nlcbsmm_init();
}
