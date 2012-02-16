/**
 * NLCBSMM Virtual Memory Manager
 */
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <vector>
#include <deque>

#include "page.h"
#include "packets.h"
#include "cln_allocator.h"
#include "pt_allocator.h"
#include "mutex.h"

namespace NLCBSMM {

   typedef
      FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_HEAP_START> > PageTableHeapType;

   // A vector of NLCBSMM::Page objects
   typedef
      std::vector<Page*,
           PageTableAllocator<Page*> > PageVectorType;

   typedef
      PageVectorType::iterator PageVectorItr;

   // A mapping of 'ip address' to a NLCBSMM::PageVector, where 'ip address' is the
   // binary representation of the IPv4 address in dot-notation.
   typedef
      std::map<uint32_t,
           PageVectorType*,
           std::less<uint32_t>,
           PageTableAllocator<std::pair<uint32_t, PageVectorType*> > > PageTableType;

   typedef
      PageTableType::iterator PageTableItr;

   // Who to contact, and with what
   typedef
      std::pair<struct sockaddr_in, Packet*> WorkTupleType;

   // A queue of work
   typedef
      std::deque<WorkTupleType*,
           CloneAllocator<WorkTupleType* > > PacketQueueType;

   typedef
      void* (*threadFunctionType) (void *);

   typedef
      int (*pthread_create_function) (pthread_t *thread,
            const pthread_attr_t *attr,
            threadFunctionType start_routine,
            void *arg);

   typedef
      void (*pthread_exit_function) (void *arg);
}

namespace NLCBSMM {

   // The node's IP address
   extern const char* local_ip;

   // The node's IP address (binary form)
   extern struct in_addr local_addr;    

   // The page table
   extern PageTableType* page_table;


   extern PacketQueueType uni_speaker_work_deque;
   extern PacketQueueType multi_speaker_work_deque;

   extern cv    uni_speaker_cond;
   extern mutex uni_speaker_cond_lock;
   extern mutex uni_speaker_lock;
   extern mutex multi_speaker_lock;

   // This forces threads to wait on each other in case someone is reading/writing the
   // page table.
   extern mutex pt_owner_lock;

   // This (binary form IP address) identifies who currently has the page table lock.
   extern uint32_t pt_owner;

   // This is a per-process lock, so the various threads don't simutaneously use the
   // page table.
   extern mutex pt_lock;

   // The next valid address to pull memory from. This is transmitted in the releaseWriteLock
   // packet.
   extern uint32_t next_addr;

   WorkTupleType* safe_pop(PacketQueueType* queue, mutex* m);

   void safe_push(PacketQueueType* queue, mutex* m, WorkTupleType* work);

   int safe_size(PacketQueueType* queue, mutex* m);

   uint32_t get_available_worker();

   PageTableHeapType* get_pt_heap(mutex* m);

   // Helper function to page align a pointer
   unsigned char* pageAlign(unsigned char* p);

   // Helper function to return page number in superblock
   unsigned int pageIndex(unsigned char* p, unsigned char* base);

   // The actual signal handler for SIGSEGV
   void signal_handler(int signo, siginfo_t* info, void* contex);

   // Registers signal handler for SIGSEGV
   void register_signal_handlers();

   // Initialize the NLCBSMM system
   void nlcbsmm_init();

}
