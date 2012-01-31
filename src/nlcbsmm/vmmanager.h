/**
 * NLCBSMM Virtual Memory Manager
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>

#include <vector>

#include "page.h"
#include "hoard_allocator.h"

#include "stlallocator.h"


namespace NLCBSMM {

   // A vector of NLCBSMM::Page objects
   typedef std::vector<Page*,
   HoardAllocator<Page*> > PageVectorType;

   // A mapping of 'ip address' to a NLCBSMM::PageVector
   typedef std::map<const char*,
   PageVectorType*,
   std::less<const char*>,
   HoardAllocator<std::pair<const char*, PageVectorType*> > > PageTableType;

   /*
      typedef FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_HEAP_START> > PageTableHeapType;

      typedef STLAllocator<Page, PageTableHeapType> PageAllocator;

      typedef std::vector<Page, PageAllocator> PageVectorType3;

      typedef STLAllocator<PageVectorType3, PageTableHeapType> PairAllocator;

      typedef std::map<const char*, PageVectorType3, std::less<const char*>, PairAllocator> PageTableType3;
    */

   typedef std::vector<Page,
           HoardAllocator<Page> > PageVectorType2;

   typedef std::map<const char*,
           PageVectorType2,
           std::less<const char*>,
           HoardAllocator<std::pair<const char*, PageVectorType2> > > PageTableType2;


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
   void spawn_listener_thread();

}
