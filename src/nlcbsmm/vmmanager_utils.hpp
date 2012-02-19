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


   bool isPageZeros(void* p) {
      /**
       * Helper function to check of a page of memory is all zeros
       */
      char testblock[PAGE_SZ] = {0};
      memset(testblock, 0, PAGE_SZ);
      return memcmp(testblock, p, PAGE_SZ) == 0;
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
         node = tuple.second;
         fprintf(stderr, "%p -> %s\n",
               (void*) (*pt_itr).first,
               inet_ntoa((struct in_addr&) node->ip_address));
      }
      fprintf(stderr, "********************\n\n");
   }


   void reserve_pages() {
      /**
       * TODO: reimplement with new data types
       */
      /*
      PageTableItr    pt_itr;
      PageVectorItr   vec_itr;
      PageVectorType* temp   = NULL;
      struct in_addr  addr   = {0};

      for (pt_itr = page_table->begin(); pt_itr != page_table->end(); pt_itr++) {
         addr.s_addr = (*pt_itr).first;
         temp = (*pt_itr).second;
         for (vec_itr = temp->begin(); vec_itr != temp->end(); vec_itr++) {
            // Reserve the memory in the virtual address space
            void* mmap_test = mmap((void*)(*vec_itr)->address,
                  PAGE_SZ,
                  PROT_NONE,
                  MAP_FIXED | MAP_ANON, -1, 0);
         }
      }
      */
   }


   uint32_t get_available_worker() {
      /**
       * Return the IP address (binary form) of the next worker capable of running a
       * new thread.
       *
       * TODO: implement this correctly (returns first non-master IP address)
       */
      MachineTableItr node_itr;
      uint32_t s_addr;

      for(node_itr = node_list->begin(); node_itr != node_list->end(); node_itr++) {
         s_addr = (*node_itr).first;
         if (s_addr != local_addr.s_addr) {
           return s_addr;
         }
      }
      return -1;
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
      //TODO: tell the associated (unicast|multicast) work deque that there is work
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

   PageTableHeapType* get_pt_heap(mutex* m) {
      PageTableHeapType* alloc;
      mutex_lock(m);
      alloc = pt_heap;
      mutex_unlock(m);
      return alloc;
   }

}
