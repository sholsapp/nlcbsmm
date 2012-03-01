/**
 * This file contains the implementation of all utility functions declared
 * in vmmanager.h.
 */
namespace NLCBSMM {

   unsigned char* pageAlign(unsigned char* p) {
      /**
       * Helper function to page align a pointer
       */
      return (unsigned char *)(((int)p) & ~(PAGE_SZ-1));
   }


   unsigned int pageIndex(unsigned char* p, unsigned char* base) {
      /**
       * Helper function to return page number in superblock
       */
      return (pageAlign(p) - base) % PAGE_SZ;
   }


   void* page_align(void* p) {
      return (unsigned char*) (((int) p) & ~(PAGE_SZ-1));
   }


   bool isPageZeros(void* p) {
      /**
       * Helper function to check of a page of memory is all zeros
       */
      char testblock[PAGE_SZ] = {0};
      memset(testblock, 0, PAGE_SZ);
      return memcmp(testblock, p, PAGE_SZ) == 0;
   }


   uint64_t get_micro_clock() {
      /**
       * Return clock time in milliseconds.
       */
      timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      return (uint64_t) ts.tv_sec * 1000000LL + (uint64_t) ts.tv_nsec / 1000LL;
   }


   const char* get_local_interface() {
      /**
       * Used to initialize the static variable local_ip with an IP address other nodes can
       *  use to contact the unicast listener.
       *
       * I'm not happy with this function still... =/ There has to be a better way to get a
       *  local interface without using string comparison and shit.
       */
      struct ifaddrs *ifaddr = NULL;
      struct ifaddrs *ifa    = NULL;
      int            family  = 0;
      int            s       = 0;
      char*          host    = (char*) clone_heap.malloc(sizeof(char) * NI_MAXHOST);

      if (getifaddrs(&ifaddr) == -1) {
         perror("getifaddrs");
         exit(EXIT_FAILURE);
      }

      // For each interface
      for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
         if (ifa->ifa_addr == NULL) {
            continue;
         }
         family = ifa->ifa_addr->sa_family;
         if (family == AF_INET) {
            if ((s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST)) != 0) {
               printf("getnameinfo() failed: %s\n", gai_strerror(s));
               exit(EXIT_FAILURE);
            }

            // Don't return the loopback interface
            if (strcmp (host, "127.0.0.1") != 0) {
               freeifaddrs(ifaddr);
               return host;
            }
         }
      }
      freeifaddrs(ifaddr);
      // This is an error case
      return "255.255.255.255";
   }


   void print_page_table() {
      /**
       *
       */
      PageTableItr pt_itr;
      PageTableElementType tuple;
      Machine*      node;

      fprintf(stderr, "**** PAGE_TABLE ****\n");
      for (pt_itr = page_table->begin(); pt_itr != page_table->end(); pt_itr++) {
         tuple = (*pt_itr).second;
         node  = tuple.second;
         fprintf(stderr, "%p -> %s\n",
               (void*) (*pt_itr).first,
               inet_ntoa((struct in_addr&) node->ip_address));
      }
      fprintf(stderr, "********************\n\n");
   }


   void reserve_pages() {
      /**
       *
       */
      PageTableItr         pt_itr;
      PageTableElementType tuple;
      Page*                page = NULL;
      Machine*             node = NULL;
      void*                test = NULL;
      void*                addr = NULL;

      fprintf(stderr, "** reserving pages **\n");
      for (pt_itr = page_table->begin();
            pt_itr != page_table->end();
            pt_itr++) {
         addr  = (void*) (*pt_itr).first;
         tuple = (*pt_itr).second;
         page  = tuple.first;
         node  = tuple.second;
         // If we aren't the owner of this page
         if (node->ip_address != local_addr.s_addr) {
            // Try to map this memory into our address space
            if((test = mmap(addr,
                        PAGE_SZ,
                        PROT_NONE,
                        MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
                        -1, 0)) == MAP_FAILED) {
               fprintf(stderr, "> %p already mapped\n", addr);
               mprotect(addr, PAGE_SZ, PROT_READ | PROT_WRITE);
            }
         }
      }
      fprintf(stderr, "*********************\n");
      return;
   }


   uint32_t get_available_worker() {
      /**
       * Return the IP address (binary form) of the next worker capable of running a
       * new thread.
       *
       * TODO: introduce new state to allow same node with extra physical cores to run
       * more than one worker at a time.
       */
      MachineTableItr node_itr;
      Machine*        machine;
      uint32_t        machine_status;
      uint32_t        ip;

      for(node_itr = node_list->begin();
            node_itr != node_list->end();
            node_itr++) {

         ip      = (*node_itr).first;
         machine = (*node_itr).second;

         // If this isn't us
         if (ip != local_addr.s_addr
               && machine->status != MACHINE_ACTIVE) {
            //fprintf(stderr, "> Available worker: %s\n",
            //      inet_ntoa((struct in_addr&) ip));
            return ip;
         }
      }
      fprintf(stderr, "> No available workers\n");
      return -1;
   }


   Machine* get_worker(uint32_t ip) {
      /**
       *
       */
      MachineTableItr node_itr;
      node_itr = node_list->find(ip);
      if (node_itr != node_list->end()) {
         return (*node_itr).second;
      }
      fprintf(stderr, "> Failed to get worker!\n");
      return NULL;
   }


   void set_new_owner(uint32_t page_addr, uint32_t ip) {
      /**
       *
       */
      Machine* worker = get_worker(ip);
      if (worker) {
         (*page_table)[page_addr].second = worker;
      }
      else {
         fprintf(stderr, "> Failed to set owner(%s)\n", inet_ntoa((struct in_addr&) ip));
      }
      return;
   }

   uint32_t get_owner(uint32_t page_addr) {
      /**
       *
       */
      return (*page_table)[page_addr].second->ip_address;
   }


   WorkTupleType* safe_pop(PacketQueueType* queue, mutex* m) {
      /**
       *
       */
      WorkTupleType* work = NULL;
      mutex_lock(m);
      work = queue->front();
      queue->pop_front();
      mutex_unlock(m);
      return work;
   }

   void safe_push(PacketQueueType* queue, mutex* m, WorkTupleType* work) {
      /**
       *
       */
      mutex_lock(m);
      queue->push_back(work);
      mutex_unlock(m);
      return;
   }

   int safe_size(PacketQueueType* queue, mutex* m) {
      /**
       *
       */
      int s = 0;
      mutex_lock(m);
      s = queue->size();
      mutex_unlock(m);
      return s;
   }

   ThreadWorkType* safe_thread_pop(ThreadQueueType* queue, mutex* m) {
      /**
       *
       */
      ThreadWorkType* work = NULL;
      mutex_lock(m);
      work = queue->front();
      queue->pop_front();
      mutex_unlock(m);
      return work;
   }

   void safe_thread_push(ThreadQueueType* queue, mutex* m, ThreadWorkType* work) {
      /**
       *
       */
      mutex_lock(m);
      queue->push_back(work);
      mutex_unlock(m);
      return;
   }

   int safe_thread_size(ThreadQueueType* queue, mutex* m) {
      /**
       *
       */
      int s = 0;
      mutex_lock(m);
      s = queue->size();
      mutex_unlock(m);
      return s;
   }

   PageTableHeapType* get_pt_heap(mutex* m) {
      PageTableHeapType* alloc;
      mutex_lock(m);
      alloc = pt_heap;
      mutex_unlock(m);
      return alloc;
   }

}
