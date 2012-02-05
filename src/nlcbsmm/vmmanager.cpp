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

#include "vmmanager.h"
#include "constants.h"
#include "mutex.h"

#define CLONE_ATTRS (CLONE_VM | CLONE_FILES | CLONE_SIGHAND | CLONE_PTRACE)
#define CLONE_MMAP_PROT_FLAGS (PROT_READ | PROT_WRITE)
#define CLONE_MMAP_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)


/**
 * Symbols defined by the end application.
 */
extern uint8_t* main;
extern uint8_t* _end;
extern uint8_t* __data_start;


namespace NLCBSMM {

   // If a network thread needs memory, it must use this private
   // heap.  This memory is lost from the DSM system.
   FirstFitHeap<NlcbsmmMmapHeap<CLONE_HEAP_START> >       clone_heap;

   // If the shared page table needs memory, it must use this
   // private heap.  This memory is kept in a fixed location in
   // memory in all instances of the application.
   FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_HEAP_START> >  pt_heap;

   // This node's ip address
   const char* local_ip = NULL;

   unsigned char* pageAlign(unsigned char* p) {
      /**
       * Helper function to page align a pointer
       */
      return (unsigned char *)(((int)p) & ~(PAGE_SZ-1));
   }

   unsigned int pageIndex(unsigned char* p, unsigned char* base) {
      /**
       * Help function to return page number in superblock
       */
      unsigned char* pageAligned = pageAlign(p);
      return (pageAlign(p) - base) % PAGE_SZ;
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

}

namespace NLCBSMM {


   PacketQueueType uni_speaker_work_deque;

   PacketQueueType multi_speaker_work_deque;

   cv    uni_speaker_cond;
   mutex uni_speaker_cond_lock;

   mutex uni_speaker_lock;
   mutex multi_speaker_lock;

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
      // Signal unicast speaker there is queued work
      cond_signal(&uni_speaker_cond);
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

}


namespace NLCBSMM {

   PageTableType* page_table;

   uint32_t _start_page_table = 0;
   uint32_t _end_page_table   = 0;
   uint32_t _uuid             = 0;
   uint32_t _next_uuid        = 1;

}

namespace NLCBSMM {

   enum multi_speaker_state { NONE, JOIN, HEARTBEAT };

   multi_speaker_state MS_STATE = NONE;

   class NetworkManager {

      public:

         NetworkManager(){
            /**
             * Constructor
             */
            multi_speaker_thread_id  = 0;
            multi_listener_thread_id = 0;
            uni_speaker_thread_id    = 0;
            uni_listener_thread_id   = 0;
         }

         ~NetworkManager(){
            /**
             * Destructor
             */
            uint32_t pid = 0;

            if((pid = waitpid(multi_speaker_thread_id, NULL,  __WCLONE)) == -1) {
               perror("wait error");
            }

            if((pid = waitpid(multi_listener_thread_id, NULL,  __WCLONE)) == -1) {
               perror("wait error");
            }

            if((pid = waitpid(uni_speaker_thread_id, NULL,  __WCLONE)) == -1) {
               perror("wait error");
            }

            if((pid = waitpid(uni_listener_thread_id, NULL,  __WCLONE)) == -1) {
               perror("wait error");
            }
         }


         void start_comms() {
            /**
             * Spawns the speaker and listener threads.
             */
            void* argument                  = NULL;
            void* multi_speaker_ptr         = NULL;
            void* multi_listener_ptr        = NULL;
            void* uni_speaker_ptr           = NULL;
            void* uni_listener_ptr          = NULL;
            void* multi_speaker_stack       = NULL;
            void* multi_listener_stack      = NULL;
            void* uni_speaker_stack         = NULL;
            void* uni_listener_stack        = NULL;
            void* multi_speaker_fixed_addr  = NULL;
            void* multi_listener_fixed_addr = NULL;
            void* uni_speaker_fixed_addr    = NULL;
            void* uni_listener_fixed_addr   = NULL;

            uni_listener_fixed_addr   = ((uint8_t*) global_base()) + (CLONE_STACK_SZ * 0);
            uni_speaker_fixed_addr    = ((uint8_t*) global_base()) + (CLONE_STACK_SZ * 1);
            multi_listener_fixed_addr = ((uint8_t*) global_base()) + (CLONE_STACK_SZ * 2);
            multi_speaker_fixed_addr  = ((uint8_t*) global_base()) + (CLONE_STACK_SZ * 3);

            argument       = (void*) 1337; // Not used

            uni_listener_stack   = (void*) mmap(uni_listener_fixed_addr,
                  CLONE_STACK_SZ,
                  CLONE_MMAP_PROT_FLAGS,
                  CLONE_MMAP_FLAGS, -1, 0);

            if (uni_listener_stack == MAP_FAILED)
               perror("!> mmap failed to allocate uni_listener_stack");

            uni_speaker_stack    = (void*) mmap(uni_speaker_fixed_addr,
                  CLONE_STACK_SZ,
                  CLONE_MMAP_PROT_FLAGS,
                  CLONE_MMAP_FLAGS, -1, 0);

            if (uni_speaker_stack == MAP_FAILED)
               perror("!> mmap failed to allocate uni_speaker_stack");

            multi_listener_stack = (void*) mmap(multi_listener_fixed_addr,
                  CLONE_STACK_SZ,
                  CLONE_MMAP_PROT_FLAGS,
                  CLONE_MMAP_FLAGS, -1, 0);

            if (multi_listener_stack == MAP_FAILED)
               perror("!> mmap failed to allocate multi_listener_stack");

            multi_speaker_stack  = (void*) mmap(multi_speaker_fixed_addr,
                  CLONE_STACK_SZ,
                  CLONE_MMAP_PROT_FLAGS,
                  CLONE_MMAP_FLAGS, -1, 0);

            if (multi_speaker_stack == MAP_FAILED)
               perror("!> mmap failed to allocate multi_speaker_stack");

            uni_speaker_ptr      = (void*) (((uint8_t*) uni_speaker_stack)    + CLONE_STACK_SZ);
            uni_listener_ptr     = (void*) (((uint8_t*) uni_listener_stack)   + CLONE_STACK_SZ);
            multi_speaker_ptr    = (void*) (((uint8_t*) multi_speaker_stack)  + CLONE_STACK_SZ);
            multi_listener_ptr   = (void*) (((uint8_t*) multi_listener_stack) + CLONE_STACK_SZ);

            if((multi_listener_thread_id =
                     clone(&multi_listener,
                        multi_listener_ptr,
                        CLONE_ATTRS,
                        argument)) == -1) {
               perror("vmmanager.cpp, clone, listener");
               exit(EXIT_FAILURE);
            }

            if((uni_listener_thread_id =
                     clone(&uni_listener,
                        uni_listener_ptr,
                        CLONE_ATTRS,
                        argument)) == -1) {
               perror("vmmanager.cpp, clone, listener");
               exit(EXIT_FAILURE);
            }

            if((multi_speaker_thread_id =
                     clone(&multi_speaker,
                        multi_speaker_ptr,
                        CLONE_ATTRS,
                        argument)) == -1) {
               perror("vmmanager.cpp, clone, speaker");
               exit(EXIT_FAILURE);
            }

            if((uni_speaker_thread_id =
                     clone(&uni_speaker,
                        uni_speaker_ptr,
                        CLONE_ATTRS,
                        argument)) == -1) {
               perror("vmmanager.cpp, clone, speaker");
               exit(EXIT_FAILURE);
            }

            // DEBUG
            //fprintf(stderr, "clone %d <%p - %p>\n", uni_listener_thread_id, uni_listener_ptr, (uint8_t*) uni_listener_ptr + CLONE_STACK_SZ);
            //fprintf(stderr, "clone %d <%p - %p>\n", uni_speaker_thread_id, uni_speaker_ptr, (uint8_t*) uni_speaker_ptr + CLONE_STACK_SZ);
            //fprintf(stderr, "clone %d <%p - %p>\n", multi_listener_thread_id, multi_listener_ptr, (uint8_t*) multi_listener_ptr + CLONE_STACK_SZ);
            //fprintf(stderr, "clone %d <%p - %p>\n", multi_speaker_thread_id, multi_speaker_ptr, (uint8_t*) multi_speaker_ptr + CLONE_STACK_SZ);

            return;
         }


         static int uni_speaker(void* t) {
            /**
             * TODO: this class is incomplete... it can only handle one type of work.
             */
            fprintf(stderr, "> uni-speaker\n");

            uint32_t               sk     = 0;
            struct sockaddr_in     addr   = {0};
            Packet*                p      = NULL;
            UnicastJoinAcceptance* uja    = NULL;
            WorkTupleType*         work   = NULL;

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("vmmanager.cpp, socket");
               exit(EXIT_FAILURE);
            }

            while(1) {

               fprintf(stderr, "> unicast waiting for work\n", safe_size(&uni_speaker_work_deque, &uni_speaker_lock));

               // If there is no work in the queue
               if (safe_size(&uni_speaker_work_deque, &uni_speaker_lock) == 0) {
                  // Wait for work (blocks until signal from other thread)
                  cond_wait(&uni_speaker_cond, &uni_speaker_cond_lock);
                  // The cond_wait specifies that it returns with second param in a locked state
                  mutex_unlock(&uni_speaker_cond_lock);
               }

               // Pop work from work queue
               work = safe_pop(&uni_speaker_work_deque, &uni_speaker_lock);

               if (work != NULL) {

                  addr = work->first;
                  p    = work->second;

                  fprintf(stderr, "> sending a packet (0x%x) to %s:%d!\n", p->get_flag(), inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

                  if (sendto(sk, p, MAX_PACKET_SZ, 0, (struct sockaddr *) &addr , sizeof(addr)) < 0) {
                     perror("vmmanager.cpp, sendto");
                     exit(EXIT_FAILURE);
                  }

                  // Another thread allocated memory and queued it for work, so
                  // free memory when we're sending it.
                  clone_heap.free(p);
                  clone_heap.free(work);
               }

               // TODO: do we need this?
               sleep(1);
            }
            return 0;
         }


         static int uni_listener(void* t) {
            /**
             *
             */
            fprintf(stderr, "> uni-listener\n");

            uint8_t* packet_buffer    = NULL;
            uint32_t sk               =  0;
            uint32_t nbytes           =  0;
            uint32_t addrlen          =  0;
            uint32_t yes              =  1;
            struct   ip_mreq mreq     = {0};
            struct   sockaddr_in addr = {0};

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("vmmanager.cpp, socket");
               exit(EXIT_FAILURE);
            }

            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port        = htons(UNICAST_PORT);
            addrlen              = sizeof(addr);

            if (bind(sk, (struct sockaddr *) &addr, addrlen) < 0) {
               perror("vmmanager.cpp, bind");
               exit(EXIT_FAILURE);
            }

            packet_buffer = (uint8_t*) clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

            while(1) {
               // Clear the memory buffer each time
               memset(packet_buffer, 0, MAX_PACKET_SZ);

               if ((nbytes = recvfrom(sk, packet_buffer, MAX_PACKET_SZ, 0, (struct sockaddr *) &addr, &addrlen)) < 0) {
                  perror("recvfrom");
                  exit(EXIT_FAILURE);
               }

               // Send buffer off for processing
               uni_listener_event_loop(packet_buffer, nbytes, addr);
            }

            // Shit is scarce, son!
            clone_heap.free(packet_buffer);
            return 0;
         }


         static void uni_listener_event_loop(void* buffer, uint32_t nbytes, struct sockaddr_in retaddr) {
            /**
             *
             */
            Packet*                p              = NULL;
            UnicastJoinAcceptance* uja            = NULL;
            SyncPage*              syncp          = NULL;
            WorkTupleType*         work           = NULL;
            uint8_t*               payload_buf    = NULL;
            uint8_t*               page_ptr       = 0;
            void*                  packet_memory  = NULL;
            void*                  work_memory    = NULL;
            void*                  page_data      = 0;
            uint32_t               i              = 0;
            uint32_t               payload_sz     = 0;
            uint32_t               page_addr      = 0;

            // Generic packet data (type/payload size/payload)
            p           = reinterpret_cast<Packet*>(buffer);
            payload_sz  = p->get_payload_sz();
            payload_buf = reinterpret_cast<uint8_t*>(p->get_payload_ptr());

            switch (p->get_flag()) {

            case UNICAST_JOIN_ACCEPT_F:
               fprintf(stderr, "> received join accept\n");

               uja = reinterpret_cast<UnicastJoinAcceptance*>(buffer);

               // record our uuid from the master
               _uuid = ntohl(uja->uuid);

               // Respond to the other server's listener
               retaddr.sin_port = htons(UNICAST_PORT);

               work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
               packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

               // Push work onto the uni_speaker's queue
               safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                     // A new work tuple
                     new (work_memory) WorkTupleType(retaddr,
                        // A new packet
                        new (packet_memory) UnicastJoinAcceptance(uja))
                     );

               MS_STATE = HEARTBEAT;
               break;

            case UNICAST_JOIN_ACCEPT_ACK_F:
               fprintf(stderr, "> received join accept ack\n");

               // Respond to the other server's listener
               retaddr.sin_port = htons(UNICAST_PORT);

               page_ptr  = reinterpret_cast<uint8_t*>(page_table);

               //fprintf(stderr, "> page_ptr = %p\n", page_ptr);

               // Queue work to send page table
               for (i = 0; i < PAGE_TABLE_SZ; i += PAGE_SZ) {

                  work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
                  packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

                  page_addr = reinterpret_cast<uint32_t>(page_ptr + i);
                  page_data = reinterpret_cast<void*>(page_ptr + i);

                  //fprintf(stderr, "> creating sync page for %p\n", (void*) page_addr);

                  // Push work onto the uni_speaker's queue
                  safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                        // A new work tuple
                        new (work_memory) WorkTupleType(retaddr,
                           // A new packet
                           new (packet_memory) SyncPage(page_addr, page_data))
                        );
               }
               break;

            case SYNC_PAGE_F:
               fprintf(stderr, "> received sync page\n");
               syncp = reinterpret_cast<SyncPage*>(buffer);
               fprintf(stderr, "> sync page at %p\n", (void*) ntohl((syncp->page_offset)));
               break;

            default:
               fprintf(stderr, "> unknown packet type (%x)\n", p->get_flag());
               break;
            }
            return;
         }


         static int multi_speaker(void* t) {
            /**
             * Initialize the multicast speaker socket and enter daemon mode.  The outer event
             * loop is responsible for determining "state" before entering an inner event loop,
             * which is responsible for implementing whatever behavior "state" might be.
             *
             * E.g., if the output loop determines the speaker's state is "HEARTBEAT", the inner
             * event loop is responsible for sending the "HEARTBEAT" packet.
             */
            fprintf(stderr, "> multi-speaker\n");

            void*               memory = NULL;
            size_t              psz    = 0;
            uint32_t            sk     = 0;
            uint32_t            cnt    = 0;
            struct ip_mreq      mreq   = {0};
            struct sockaddr_in  addr   = {0};
            Packet*             p      = NULL;

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("vmmanager.cpp, socket");
               exit(EXIT_FAILURE);
            }

            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = inet_addr(MULTICAST_GRP);
            addr.sin_port        = htons(MULTICAST_PORT);

            // This buffer is big enough to hold the biggest packet
            psz     = MAX_PACKET_SZ;
            memory  = clone_heap.malloc(psz);

            // Initial state
            MS_STATE = JOIN;

            for (cnt = 0; ; cnt++) {
               // Clear the memory buffer each time
               memset(memory, 0, psz);

               // If in JOIN state and received no response from a master
               if (MS_STATE == JOIN && cnt > MAX_JOIN_ATTEMPTS) {
                  if (_uuid == (uint32_t) -1) {
                     // I am master
                     _uuid = 0;
                  }
                  // Go to HEARTBEAT state
                  // TODO: maybe enter a TIMEOUT state?
                  MS_STATE = HEARTBEAT;
               }

               multi_speaker_event_loop(memory, psz, sk, addr);

               // TODO: replace sleeping with checking the work queue for work?
               sleep(1);

            }

            // Shit is scarce, son!
            clone_heap.free(memory);
            return 0;
         }


         static void multi_speaker_event_loop(void* buffer, size_t buffer_sz, uint32_t sk, struct sockaddr_in&  addr) {
            /**
             *
             */
            Packet* p    = NULL;

            switch (MS_STATE) {

            case JOIN:
               // Build packet
               p = new (buffer) MulticastJoin(strlen(local_ip), &main, &_end, (uint8_t*) global_base());
               // Add packet payload (the user IP address)
               memcpy(p->get_payload_ptr(), local_ip, strlen(local_ip));
               break;

            case HEARTBEAT:
               // Build packet
               p = new (buffer) MulticastHeartbeat(strlen(local_ip));
               // Add packet payload (the user IP address)
               memcpy(p->get_payload_ptr(), local_ip, strlen(local_ip));
               break;

            }

            // Send whatever we just built
            if (sendto(sk, buffer, buffer_sz, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
               perror("vmmanager.cpp, sendto");
               exit(EXIT_FAILURE);
            }

            return;
         }


         static int multi_listener(void* t) {
            /**
             *
             */
            fprintf(stderr, "> multi-listener\n");

            uint8_t  *packet_buffer      = NULL;
            uint32_t sk                  =  0;
            uint32_t nbytes              =  0;
            uint32_t addrlen             =  0;
            uint32_t yes                 =  1;
            struct   ip_mreq mreq        = {0};
            struct   sockaddr_in addr    = {0};

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("vmmanager.cpp, socket");
               exit(EXIT_FAILURE);
            }

            // Allow multiple sockets to use the same port
            if (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
               perror("vmmanager.cpp, setsockopt");
               exit(EXIT_FAILURE);
            }

            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port        = htons(MULTICAST_PORT);
            addrlen              = sizeof(addr);

            if (bind(sk, (struct sockaddr *) &addr, addrlen) < 0) {
               perror("vmmanager.cpp, bind");
               exit(EXIT_FAILURE);
            }

            // Use setsockopt() to join multicast group
            mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GRP);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            if (setsockopt(sk, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
               perror("vmmanager.cpp, setsockopt");
               exit(EXIT_FAILURE);
            }

            // Get a new buffer from our allocator
            packet_buffer = (uint8_t*) clone_heap.malloc(MAX_PACKET_SZ);

            while (1) {

               // Clear the buffer
               memset(packet_buffer, 0, MAX_PACKET_SZ);

               if ((nbytes = recvfrom(sk, packet_buffer, MAX_PACKET_SZ, 0, (struct sockaddr *) &addr, &addrlen)) < 0) {
                  perror("recvfrom");
                  exit(EXIT_FAILURE);
               }

               // Send the buffer off for processing
               multi_listener_event_loop(packet_buffer, nbytes);

            }
            // Shit is scarce, son!
            clone_heap.free(packet_buffer);
            return 0;
         }


         static void multi_listener_event_loop(void* buffer, uint32_t nbytes) {
            /**
             * Process the packet in buffer (which is nbytes long) and take appropriate
             * action (if applicable).
             *
             * E.g., if we receive a packet of type MULTICAST_JOIN_F, then verify address
             * space information, build a UnicastJoinAcceptance packet, queue the new packet
             * for process in the unicast_speaker's work queue, and return.
             */
            Packet*                p              = NULL;
            MulticastJoin*         mjp            = NULL;
            MulticastHeartbeat*    mjh            = NULL;
            UnicastJoinAcceptance* uja            = NULL;
            WorkTupleType*         work           = NULL;
            void*                  packet_memory  = NULL;
            void*                  work_memory    = NULL;
            char*                  payload_buf    = NULL;
            uint32_t               payload_sz     = 0;
            struct sockaddr_in     retaddr        = {0};

            // Generic packet data (type/payload size/payload)
            p           = reinterpret_cast<Packet*>(buffer);
            payload_sz  = p->get_payload_sz();
            payload_buf = reinterpret_cast<char*>(p->get_payload_ptr());

            switch (p->get_flag()) {

               // Some sent a request to join the cluster.
            case MULTICAST_JOIN_F:

               mjp = reinterpret_cast<MulticastJoin*>(buffer);

               // If we're the master
               if (_uuid == 0) {

                  fprintf(stderr, "> %s trying to join...\n", payload_buf);

                  // Verify request's address space requirements
                  if ((uint32_t) &main == ntohl(mjp->main_addr)
                        && (uint32_t) &_end == ntohl(mjp->end_addr)
                        && (uint32_t) global_base() == ntohl(mjp->prog_break_addr)) {

                     // Allocate memory for the new work/packet
                     work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
                     packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

                     // Who to contact
                     retaddr.sin_family      = AF_INET;
                     retaddr.sin_addr.s_addr = inet_addr(payload_buf);
                     retaddr.sin_port        = htons(UNICAST_PORT);

                     // Push work onto the uni_speaker's queue
                     safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                           // A new work tuple
                           new (work_memory) WorkTupleType(retaddr,
                              // A new packet
                              new (packet_memory) UnicastJoinAcceptance(strlen(local_ip),
                                 _start_page_table,
                                 _end_page_table,
                                 _next_uuid++))
                           );
                  }
                  else {
                     fprintf(stderr, "> invalid address space detected\n");
                  }
               }
               break;

            case MULTICAST_HEARTBEAT_F:
               mjh = reinterpret_cast<MulticastHeartbeat*>(buffer);
               fprintf(stderr, "%s: <3\n", payload_buf);
               break;

            default:
               break;

            }
         }

      private:

         uint32_t multi_speaker_thread_id;
         uint32_t multi_listener_thread_id;
         uint32_t uni_speaker_thread_id;
         uint32_t uni_listener_thread_id;
   };


   /**
    * This is the distributed system server
    */
   NetworkManager networkmanager;


   void signal_handler(int signo, siginfo_t* info, void* contex) {
      /**
       * The actual signal handler for SIGSEGV
       */
      sigset_t oset;
      sigset_t set;

      //block SIGSEGV
      sigemptyset(&set);
      sigaddset(&set, SIGSEGV);
      sigprocmask(SIG_BLOCK, &set, &oset);

      //fprintf(stderr, "SIGSEGV Caught\n");

      unsigned char* p = pageAlign((unsigned char*) info->si_addr);
      fprintf(stderr, "Illegal access at %p in page %p\n", info->si_addr, p);

      // Unblock sigsegv
      sigprocmask(SIG_UNBLOCK, &set, &oset);
   }


   void register_signal_handlers() {
      /**
       * Registers signal handler for SIGSEGV
       */
      //fprintf(stderr, "Registering SIGSEGV handler...");
      struct sigaction act;
      act.sa_sigaction = signal_handler;
      act.sa_flags     = SA_SIGINFO;
      sigemptyset(&act.sa_mask);
      sigaction(SIGSEGV, &act, 0);
      //fprintf(stderr, "done\n");
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
      fprintf(stdout, "> main (%p) | _end (%p)\n",          &main, &_end);
      fprintf(stdout, "> heap obj (ch) lives in %p <\n",    &clone_heap);
      fprintf(stdout, "> heap obj (pth) lives in %p <\n",   &pt_heap);
      fprintf(stdout, "> page table lives in %p - %p <\n",  (void*) _start_page_table, (void*) _end_page_table);
      print_log_sep(40);
      fprintf(stdout, "> base %p (thread stacks go here) sbrk(0) = %p\n", (void*) global_base(), sbrk(0));
      fprintf(stdout, "> clone alloc heap offset %p\n",      (void*) global_clone_alloc_heap());
      fprintf(stdout, "> clone heap offset %p\n",            (void*) global_clone_heap());
      fprintf(stdout, "> page table offset %p\n",            (void*) global_page_table());
      fprintf(stdout, "> page table alloc heap offset %p\n", (void*) global_page_table_alloc_heap());
      fprintf(stdout, "> page table heap offset %p\n",       (void*) global_page_table_heap());
      print_log_sep(40);
      return;
   }


   void nlcbsmm_init() {
      /**
       * Hook entry.
       */

      // Dedicated memory to maintaining the page table
      void* raw         = (void*) mmap((void*) global_page_table(),
            PAGE_TABLE_SZ,
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
            -1, 0);
      page_table        = new (raw) PageTableType();
      _start_page_table = (uint32_t) raw;
      _end_page_table   = (uint32_t) ((uint8_t*) raw) + PAGE_TABLE_SZ;
      _uuid             = (uint32_t) -1;

      // Obtain the IP address of the local ethernet interface
      local_ip = get_local_interface();

      // Setup condition and mutex variables
      cond_init(&uni_speaker_cond,       NULL);
      mutex_init(&uni_speaker_cond_lock, NULL);
      mutex_init(&uni_speaker_lock,      NULL);
      mutex_init(&multi_speaker_lock,    NULL);

      print_init_message();

      // Debug
      //(*page_table)["127.0.0.1"] = new (pt_heap.malloc(sizeof(PageVectorType))) PageVectorType();
      //(*page_table)["127.0.0.1"]->push_back(new (pt_heap.malloc(sizeof(Page))) Page(666, PROT_NONE));
      //(*page_table)["127.0.0.2"] = new (pt_heap.malloc(sizeof(PageVectorType))) PageVectorType();
      //(*page_table)["127.0.0.2"]->push_back(new (pt_heap.malloc(sizeof(Page))) Page(777, PROT_NONE));
      //(*page_table)["127.0.0.3"] = new (pt_heap.malloc(sizeof(PageVectorType))) PageVectorType();
      //(*page_table)["127.0.0.3"]->push_back(new (pt_heap.malloc(sizeof(Page))) Page(888, PROT_NONE));

      //fprintf(stderr, "1 > %d\n", (*page_table)["127.0.0.1"]->at(0)->address);
      //fprintf(stderr, "2 > %d\n", (*page_table)["127.0.0.2"]->at(0)->address);
      //fprintf(stderr, "3 > %d\n", (*page_table)["127.0.0.3"]->at(0)->address);
      // End Debug

      // Register SIGSEGV handler
      //register_signal_handlers();

      // Spawn the thread that speaks/listens to cluster
      networkmanager.start_comms();
   }

}
