#include <cstdio>
#include <cstring>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <ifaddrs.h>
#include <cstdlib>
#include <unistd.h>
#include <netdb.h>
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <execinfo.h>

#include "vmmanager.h"
#include "constants.h"


namespace NLCBSMM {

   bool ready = false;

   // If a network thread needs memory, it must use this private
   // heap.  This memory is lost from the DSM system.
   FirstFitHeap<NlcbsmmMmapHeap<CLONE_HEAP_START> > clone_heap;

   // If the shared page table needs memory, it must use this
   // private heap.  This memory is kept in a fixed location in
   // memory in all instances of the application.
   PageTableHeapType* pt_heap;

   const char*    local_ip   = NULL;
   struct in_addr local_addr = {0};

   AddressListType       invalidated;
   InvalidationTableType mutex_rc_map;
   MutexTableType        mutex_map;
   mutex                 mutex_map_lock;

   ThreadTableType  thread_map;
   mutex            thread_map_lock;

   ThreadQueueType  thread_deque;
   cv               thread_cond;
   mutex            thread_cond_lock;
   mutex            thread_deque_lock;

   // This set of condition variables and mutex allows us to shut down the unicasts speaker
   // while there is no work for it to perform in its work queue.
   PacketQueueType uni_speaker_work_deque;
   cv              uni_speaker_cond;
   mutex           uni_speaker_cond_lock;
   mutex           uni_speaker_lock;

   // This set of condition variables adn mutex allows us to shut down the multicast speaker
   // while there is no work for it to perform in its work queue.
   //TODO: make this look like unicast speaker (above)
   PacketQueueType multi_speaker_work_deque;
   mutex           multi_speaker_lock;

   // This (binary form IP address) identifies who currently has the page table lock.
   int pt_owner;
   // This forces threads to wait on each other in case someone is reading/writing the
   // page table.
   mutex pt_owner_lock;
   // This is a per-process lock, so the various threads don't simutaneously use the
   // page table.
   mutex pt_lock;

   // This is a per-process lock, so the various threads don't simutaneously malloc
   // and free from the pt heap.
   //mutex pt_heap_lock;
   // This is a per-process lock, so the various threads don't simutaneously malloc
   // and free from the clone heap.
   //mutex clone_heap_lock;

   mutex node_list_lock;

   MachineTableType* node_list;
   PageTableType*    page_table;

   intptr_t _start_page_table = 0;
   intptr_t _end_page_table   = 0;
   int _uuid             = 0;
   int _next_uuid        = 1;
   intptr_t next_addr         = 0;
}

// The implementation of utility functions defined in this file
#include "vmmanager_utils.hpp"

namespace NLCBSMM {

   enum multi_speaker_state { NONE, JOIN, HEARTBEAT };

   multi_speaker_state MS_STATE = NONE;

   /**
    * This is the distributed system server
    */
   ClusterCoordinator networkmanager;


   void signal_handler(int signo, siginfo_t* info, void* contex) {
      /**
       * The actual signal handler for SIGSEGV
       */
      sigset_t oset;
      sigset_t set;

      void*                 packet_memory  = NULL;
      void*                 raw            = NULL;
      void*                 test           = NULL;
      void*                 rel_page       = NULL;
      uint8_t*              faulting_addr  = NULL;
      uint8_t*              aligned_addr   = NULL;

      int              remote_ip      = 0;
      int              timeout        = 0;
      int              perm           = 0;
      struct sockaddr_in    remote_addr    = {0};
      struct sockaddr_in    to             = {0};
      struct sockaddr_in    from           = {0};
      Packet*               p              = NULL;
      ThreadCreate*         tc             = NULL;
      ThreadCreateAck*      tca            = NULL;
      AcquirePage*          ap             = NULL;
      ReleasePage*          rp             = NULL;
      Machine*              node           = NULL;
      Page*                 page           = NULL;
      PageTableItr          pt_itr;
      PageTableElementType  tuple;

      int sk = 0;

      uint64_t start, end;

      start = get_micro_clock();

      // Block SIGSEGV while executing this
      sigemptyset(&set);
      sigaddset(&set, SIGSEGV);
      sigprocmask(SIG_BLOCK, &set, &oset);

      faulting_addr = reinterpret_cast<uint8_t*>(info->si_addr);
      aligned_addr  = pageAlign(faulting_addr);

      mutex_lock(&pt_lock);
      pt_itr = page_table->find((intptr_t) aligned_addr);
      mutex_unlock(&pt_lock);

      // If the address was not found in the page table
      if(pt_itr == page_table->end()) {
         // This is a real segfault
         fprintf(stderr,"> SEGFAULT: %p\n", aligned_addr);
         exit(EXIT_FAILURE);
      }

      tuple = (*pt_itr).second;
      page  = tuple.first;
      perm  = page->protection;
      node  = tuple.second;

      if (node->ip_address == local_addr.s_addr) {
         fprintf(stderr, "> %p invalidated - granting PROT_WRITE!\n", (void*) page->address);
         if(mprotect((void*) page->address,
                  PAGE_SZ,
                  PROT_READ | PROT_WRITE) < 0) {
            fprintf(stderr, "> Fault: mprotect failed\n");
         }
         // Invalidate page
         invalidated.push_back(page->address);

         return;
      }

      remote_ip                   = node->ip_address;
      remote_addr.sin_family      = AF_INET;
      remote_addr.sin_addr.s_addr = remote_ip;
      remote_addr.sin_port        = htons(UNICAST_PORT);

      // Debug (for printing)
      from = remote_addr;
      to.sin_family = AF_INET;
      to.sin_addr   = local_addr;
      to.sin_port   = 0;

      sk = ClusterCoordinator::new_comm();

      timeout = 5;


      packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
      p = ClusterCoordinator::persistent_blocking_comm(sk,
            (struct sockaddr*) &remote_addr,
            reinterpret_cast<Packet*>(
               new (packet_memory) AcquirePage((intptr_t) aligned_addr)),
            timeout,
            "acquire page"
            );

      //fprintf(stderr, "> Asked %s for %p\n",
      //      inet_ntoa((struct in_addr&) remote_ip),
      //      (void*) aligned_addr);

      // If we received a response
      if (p) {

         if (p->get_flag() == SYNC_RELEASE_PAGE_F) {
            rp = reinterpret_cast<ReleasePage*>(p);

            rel_page = (void*) ntohl(rp->page_addr);

            // Memory should be mapped, set permissions
            if(mprotect(rel_page,
                     PAGE_SZ,
                     PROT_READ | PROT_WRITE) < 0) {
               fprintf(stderr, "> Fault: mprotect failed\n");
            }
            // Copy page data
            memcpy(rel_page, p->get_payload_ptr(), PAGE_SZ);

            // Set new owner (us)
            set_new_owner((intptr_t) rel_page, local_addr.s_addr);

            packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
            ClusterCoordinator::direct_comm(remote_addr,
                  new (packet_memory) GenericPacket(SYNC_RELEASE_PAGE_ACK_F));

            // Memory should be mapped, set permissions
            if(mprotect(rel_page,
                     PAGE_SZ,
                     PROT_READ) < 0) {
               fprintf(stderr, "> Fault: mprotect failed\n");
            }



         }

         // TODO: Add a multicat packet to inform the other hosts
         // that I am the new owner of the page p (or let loser do this?)
         // TODO: increment the version of the page?

         end = get_micro_clock();

         fprintf(stdout, "SEGV|%lld|%s|%s|%p|%lld\n",
               get_micro_clock(),
               inet_ntoa(to.sin_addr),
               inet_ntoa(from.sin_addr),
               rel_page,
               (end - start));
         fflush(stdout);

         // Free the packet
         clone_heap.free(p);
      }
      else {
         fprintf(stderr, "> SEGFAULT (couldn't resolve %p)\n", aligned_addr);
         exit(EXIT_FAILURE);
      }

      // Unblock sigsegv
      sigprocmask(SIG_UNBLOCK, &set, &oset);
      close(sk);

      return;
   }


   void register_signal_handlers() {
      /**
       * Registers signal handler for SIGSEGV
       */
      fprintf(stderr, "Registering SIGSEGV handler...");
      struct sigaction act;
      act.sa_sigaction = signal_handler;
      act.sa_flags     = SA_SIGINFO;
      sigemptyset(&act.sa_mask);
      sigaction(SIGSEGV, &act, 0);
      fprintf(stderr, "done\n");
      return;
   }


   void print_log_sep(int len) {
      /**
       *
       */
      fprintf(stdout, "\n");
      for (int i = 0; i < len; i++) {
         fprintf(stderr, "%c", (char) 144);
      }
      fprintf(stdout, "\n\n");
   }


   void print_init_message() {
      /**
       *
       */
      print_log_sep(40);
      fprintf(stdout, "> nlcbsmm init on local ip: %s <\n", local_ip);
      fprintf(stdout, "> main (%p) | _end (%p)\n",           (void*) global_main(),
            (void*) global_end());
      fprintf(stdout, "> base %p (thread stacks go here)\n", (void*) global_base());
      fprintf(stdout, "> clone alloc heap offset %p\n",      (void*) global_clone_alloc_heap());
      fprintf(stdout, "> clone heap offset %p\n",            (void*) global_clone_heap());
      fprintf(stdout, "> page table obj offset %p\n",        (void*) global_page_table_obj());
      fprintf(stdout, "> page table offset %p\n",            (void*) global_page_table());
      fprintf(stdout, "> page table alloc heap offset %p\n", (void*) global_page_table_alloc_heap());
      fprintf(stdout, "> page table heap offset %p\n",       (void*) global_page_table_heap());
      print_log_sep(40);
      return;
   }


   void nlcbsmm_init_locks() {
      /**
       * Setup mutex and cond vars
       */
      cond_init(&uni_speaker_cond,       NULL);
      cond_init(&thread_cond,            NULL);
      mutex_init(&uni_speaker_cond_lock, NULL);
      mutex_init(&uni_speaker_lock,      NULL);
      mutex_init(&multi_speaker_lock,    NULL);
      mutex_init(&pt_owner_lock,         NULL);
      mutex_init(&pt_lock,               NULL);
      mutex_init(&thread_cond_lock,      NULL);
      mutex_init(&thread_deque_lock,     NULL);
      mutex_init(&thread_map_lock,       NULL);
      //mutex_init(&pt_heap_lock,          NULL);
      //mutex_init(&clone_heap_lock,       NULL);
      mutex_init(&node_list_lock,        NULL);
      mutex_init(&mutex_map_lock,        NULL);
      return;
   }

   /*void* pt_heap_malloc(size_t sz) {
     void* ret;
     mutex_lock(&pt_heap_lock);
     ret = pt_heap->malloc(sz);
     mutex_unlock(&pt_heap_lock);
     return ret;
     }

     void pt_heap_free(void* addr) {
     mutex_lock(&pt_heap_lock);
     pt_heap->free(addr);
     mutex_unlock(&pt_heap_lock);
     }

     void* clone_heap_malloc(size_t sz) {
     void* ret;
     mutex_lock(&clone_heap_lock);
     ret = clone_heap.malloc(sz);
     mutex_unlock(&clone_heap_lock);
     return ret;
     }

     void clone_heap_free(void* addr) {
     mutex_lock(&clone_heap_lock);
     clone_heap.free(addr);
     mutex_unlock(&clone_heap_lock);
     }*/


   void nlcbsmm_init_heaps() {
      /**
       * Cheap hack, but forces heaps to initialize memory pools.
       */
      //pt_heap_free(pt_heap_malloc(8));
      pt_heap->free(pt_heap->malloc(8));
      //clone_heap_free(clone_heap_malloc(8));
      clone_heap.free(clone_heap.malloc(8));
      return;
   }


   void nlcbsmm_init() {
      /**
       * Hook entry.
       */

      void* raw = NULL;

      // Used in the heap allocators
      next_addr = global_application_heap();

      if ((raw = (void*) mmap((void*) global_page_table_obj(),
                  PAGE_TABLE_OBJ_SZ,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
                  -1, 0)) == MAP_FAILED) {
         perror("mmap(gpto)");
      }
      pt_heap = new (raw) PageTableHeapType();

      // Force init (must follow init of pt_heap)
      nlcbsmm_init_heaps();

      // Dedicated memory for maintaining the machine list
      if ((raw = (void*) mmap((void*) global_page_table_mach_list(),
                  PAGE_TABLE_MACH_LIST_SZ,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
                  -1, 0)) == MAP_FAILED) {
         perror("mmap(gptml)");
      }
      node_list = new (raw) MachineTableType();

      // Dedicated memory for maintaining the page table
      if ((raw = (void*) mmap((void*) global_page_table(),
                  PAGE_TABLE_SZ,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
                  -1, 0)) == MAP_FAILED) {
         perror("mmap(gpt)");
      }
      page_table = new (raw) PageTableType();

      _start_page_table = (intptr_t) raw;
      _end_page_table   = (intptr_t) ((uint8_t*) raw) + PAGE_TABLE_SZ;
      _uuid             = (int) -1;
      pt_owner          = (int) -1;
      local_ip          = get_local_interface();
      local_addr.s_addr = inet_addr(local_ip);

      nlcbsmm_init_locks();

      // Register SIGSEGV handler
      register_signal_handlers();

      // Debug
      print_init_message();

      // Spawn the thread that speaks/listens to cluster
      networkmanager.start_comms();

      ready = true;
   }

}
