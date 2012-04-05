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
#include "pthread_work.h"

#include "cln_allocator.h"
#include "pt_allocator.h"

#include "mutex.h"

#include "cluster.h"

namespace NLCBSMM {

   typedef uint32_t ip4_addr;
   typedef uint32_t threadid;

   typedef
      FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_HEAP_START> > PageTableHeapType;

   /**
    * Machine table types
    */
   typedef
      std::map<ip4_addr, Machine*,
      std::less<ip4_addr>,
      PageTableAllocator<std::pair<ip4_addr, Machine* > > >
         MachineTableType;
   typedef
      MachineTableType::iterator MachineTableItr;

   /**
    * Page table types
    */
   typedef
      std::pair<Page*, Machine*> PageTableElementType;
   typedef
      std::map<intptr_t, PageTableElementType,
      std::less<intptr_t>,
      PageTableAllocator<std::pair<intptr_t, PageTableElementType > > >
         PageTableType;
   typedef
      PageTableType::iterator PageTableItr;

   /**
    * Thread table type
    */
   typedef
      std::map<threadid, struct sockaddr,
      std::less<threadid>,
      CloneAllocator<std::pair<threadid, struct sockaddr> > >
         ThreadTableType;

   /**
    * Mutex table type
    */
   typedef
      std::vector<vmaddr_t,
      CloneAllocator<vmaddr_t> > AddressListType;
   typedef
      std::map<vmaddr_t, AddressListType,
      std::less<vmaddr_t>,
      CloneAllocator<std::pair<vmaddr_t, AddressListType> > >
         InvalidationTableType;
   typedef
      std::deque<struct sockaddr_in,
      CloneAllocator<struct sockaddr_in> >
         WaitQueue;
   typedef
      std::map<vmaddr_t, WaitQueue,
      std::less<vmaddr_t>,
      CloneAllocator<std::pair<vmaddr_t, WaitQueue> > >
         MutexTableType;

   /**
    * Work queue type
    */
   typedef
      std::pair<struct sockaddr_in, Packet*> WorkTupleType;
   typedef
      std::deque<WorkTupleType*,
      CloneAllocator<WorkTupleType* > > PacketQueueType;

   /**
    * Thread work type
    */
   typedef
      std::pair<struct sockaddr_in, PthreadWork> ThreadWorkType;
   typedef
      std::deque<ThreadWorkType*,
      CloneAllocator<ThreadWorkType* > > ThreadQueueType;


   /**
    * The pthread library function signatures
    */
   typedef
      void* (*threadFunctionType) (void *);

   typedef
      int (*pthread_create_function) (pthread_t *thread,
            const pthread_attr_t *attr,
            threadFunctionType start_routine,
            void *arg);

   typedef
      void (*pthread_exit_function) (void *arg);

   typedef
      int (*pthread_join_function) (pthread_t thread,
            void** value_ptr);

   typedef
      int (*pthread_mutex_init_function) (pthread_mutex_t *mutex,
            const pthread_mutexattr_t *attr);

   typedef
      int (*pthread_mutex_destroy_function) (pthread_mutex_t *mutex);

   // TODO: what is this?
   // pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

   typedef
      int (*pthread_mutex_lock_function) (pthread_mutex_t *mutex);

   typedef
      int (*pthread_mutex_unlock_function) (pthread_mutex_t *mutex);

   typedef
      int (*pthread_mutex_trylock_function) (pthread_mutex_t *mutex);

}

namespace NLCBSMM {

   extern bool ready;

   // The node's IP address
   extern const char* local_ip;
   extern struct in_addr local_addr;

   // The page table (heap allocated, sync'd)
   extern PageTableType*     page_table;
   extern MachineTableType*  node_list;

   // Master state (static allocation, not sync'd)
   extern ThreadTableType       thread_map;
   extern MutexTableType        mutex_map;
   extern InvalidationTableType mutex_rc_map;
   extern AddressListType       invalidated;

   // The work queues
   extern PacketQueueType uni_speaker_work_deque;
   extern PacketQueueType multi_speaker_work_deque;

   extern ThreadQueueType thread_deque;

   // The locks
   extern cv    uni_speaker_cond;
   extern cv    thread_cond;
   extern mutex uni_speaker_cond_lock;
   extern mutex uni_speaker_lock;
   extern mutex multi_speaker_lock;
   extern mutex thread_cond_lock;
   extern mutex thread_deque_lock;
   extern mutex node_list_lock;
   extern mutex mutex_map_lock;

   // This forces threads to wait on each other in case someone is reading/writing the
   // page table.
   extern mutex pt_owner_lock;

   // This (binary form IP address) identifies who currently has the page table lock.
   extern ip4_addr pt_owner;

   // This is a per-process lock, so the various threads don't simutaneously use the
   // page table.
   extern mutex pt_lock;

   // This is a per-process lock, so the various threads don't simutaneously malloc
   // and free from the pt heap.
   //extern mutex pt_heap_lock;
   // This is a per-process lock, so the various threads don't simutaneously malloc
   // and free from the clone heap.
   //extern mutex clone_heap_lock;


   // The next valid address to pull memory from. This is transmitted in the releaseWriteLock
   // packet.
   extern intptr_t next_addr;
   extern intptr_t _start_page_table;
   extern intptr_t _end_page_table;
   extern intptr_t _uuid;
   extern intptr_t _next_uuid;

   // Function prototypes
   WorkTupleType*     safe_pop(PacketQueueType* queue, mutex* m);
   void               safe_push(PacketQueueType* queue, mutex* m, WorkTupleType* work);
   int                safe_size(PacketQueueType* queue, mutex* m);

   ThreadWorkType*    safe_thread_pop(ThreadQueueType* queue, mutex* m);
   void               safe_thread_push(ThreadQueueType* queue, mutex* m, ThreadWorkType* work);
   int                safe_thread_size(ThreadQueueType* queue, mutex* m);


   //void*              pt_heap_malloc(uint32_t sz);
   //void               pt_heap_free(void* addr);
   //void*              clone_heap_malloc(uint32_t sz);
   //void               clone_heap_free(void* addr);

   Machine* get_worker(ip4_addr ip); 
   PageTableHeapType* get_pt_heap(mutex* m);
   ip4_addr           get_available_worker();
   void               signal_handler(int signo, siginfo_t* info, void* contex);
   void               register_signal_handlers();
   void               nlcbsmm_init();
   void               print_page_table();
   void               reserve_pages();
   void               set_new_owner(intptr_t page_addr, ip4_addr ip);
   uint64_t           get_micro_clock();
   void*              page_align(void* p);
   // TODO: Fix naming convention
   unsigned int       pageIndex(unsigned char* p, unsigned char* base);
   unsigned char*     pageAlign(unsigned char* p);
   bool               isPageZeros(void* p);

}
