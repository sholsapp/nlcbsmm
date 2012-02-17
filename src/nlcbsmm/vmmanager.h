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
#include "machine.h"
#include "packets.h"

#include "cln_allocator.h"
#include "pt_allocator.h"

#include "mutex.h"

#include "cluster.h"

namespace NLCBSMM {

   typedef
      FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_HEAP_START> > PageTableHeapType;

   //
   // Page table types
   //
   typedef
      std::map<uint32_t,
      Machine*,
      std::less<uint32_t>,
      PageTableAllocator<std::pair<uint32_t, Machine* > > >
         MachineTableType;

   typedef
      MachineTableType::iterator MachineTableItr;

   typedef
      std::pair<Page*, Machine*> PageTableElementType;

   typedef
      std::map<uint32_t,
      PageTableElementType,
      std::less<uint32_t>,
      PageTableAllocator<std::pair<uint32_t, PageTableElementType > > >
         PageTableType;

   typedef
      PageTableType::iterator PageTableItr2;

   //
   // Work queue types
   //
   typedef
      std::pair<struct sockaddr_in, Packet*> WorkTupleType;

   typedef
      std::deque<WorkTupleType*,
      CloneAllocator<WorkTupleType* > > PacketQueueType;


   //
   // The pthread library function signatures
   //
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
   extern struct in_addr local_addr;

   // The page table
   extern PageTableType* page_table;
   extern MachineTableType* node_list;


   // The work queues
   extern PacketQueueType uni_speaker_work_deque;
   extern PacketQueueType multi_speaker_work_deque;

   // The locks
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

   extern uint32_t _start_page_table;
   extern uint32_t _end_page_table;
   extern uint32_t _uuid;
   extern uint32_t _next_uuid;

   extern uint32_t next_addr;


   // Function prototypes
   WorkTupleType*     safe_pop(PacketQueueType* queue, mutex* m);
   void               safe_push(PacketQueueType* queue, mutex* m, WorkTupleType* work);
   int                safe_size(PacketQueueType* queue, mutex* m);
   PageTableHeapType* get_pt_heap(mutex* m);
   uint32_t           get_available_worker();
   unsigned char*     pageAlign(unsigned char* p);
   unsigned int       pageIndex(unsigned char* p, unsigned char* base);
   void               signal_handler(int signo, siginfo_t* info, void* contex);
   void               register_signal_handlers();
   void               nlcbsmm_init();
   void               print_page_table();
   bool               isPageZeros(void* p);

}
