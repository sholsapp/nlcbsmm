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

#include "vmmanager.h"
#include "constants.h"
//#include "mutex.h"

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
   FirstFitHeap<NlcbsmmMmapHeap<CLONE_HEAP_START> > clone_heap;

   // If the shared page table needs memory, it must use this
   // private heap.  This memory is kept in a fixed location in
   // memory in all instances of the application.
   PageTableHeapType* pt_heap;

   // This node's ip address
   const char* local_ip = NULL;
   // TODO: replace all instances of local_ip with local_addr (binary form ip)
   struct in_addr local_addr = {0};

   // This set of condition variables and mutex allows us to shut down the unicasts speaker
   // while there is no work for it to perform in its work queue.
   PacketQueueType uni_speaker_work_deque;
   cv              uni_speaker_cond;
   mutex           uni_speaker_cond_lock;
   mutex           uni_speaker_lock; // This locks the queue during push/pop/size ops

   // This set of condition variables adn mutex allows us to shut down the multicast speaker
   // while there is no work for it to perform in its work queue.
   //TODO: make this look like unicast speaker (above)
   PacketQueueType multi_speaker_work_deque;
   mutex           multi_speaker_lock; // This locks the queue during push/pop/size ops

   // This (binary form IP address) identifies who currently has the page table lock.
   uint32_t pt_owner;
   // This forces threads to wait on each other in case someone is reading/writing the
   // page table.
   mutex pt_owner_lock;
   // This is a per-process lock, so the various threads don't simutaneously use the
   // page table.
   mutex pt_lock;

   PageTableType* page_table;

   uint32_t _start_page_table = 0;
   uint32_t _end_page_table   = 0;
   uint32_t _uuid             = 0;
   uint32_t _next_uuid        = 1;

}

// The implementation of utility functions defined in this file
#include "vmmanager_utils.hpp"

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
                  // free memory when we're done sending it.
                  clone_heap.free(p);
                  clone_heap.free(work);
               }
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
            ThreadCreate*          tc             = NULL;
            WorkTupleType*         work           = NULL;
            uint8_t*               payload_buf    = NULL;
            uint8_t*               page_ptr       = NULL;
            void*                  packet_memory  = NULL;
            void*                  work_memory    = NULL;
            void*                  page_data      = NULL;
            void*                  thr_stack      = NULL;
            void*                  thr_stack_ptr  = NULL;
            void*                  func           = NULL;
            void*                  arg            = NULL;
            uint32_t               i              = 0;
            uint32_t               region_sz      = 0;
            uint32_t               payload_sz     = 0;
            uint32_t               page_addr      = 0;
            uint32_t               thr_id         = 0;


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
               // record the current pt_owner
               pt_owner = ntohl(uja->pt_owner);

               // How big is the region we're sync'ing?
               region_sz = PAGE_TABLE_OBJ_SZ
                  + PAGE_TABLE_SZ
                  + PAGE_TABLE_ALLOC_HEAP_SZ
                  + PAGE_TABLE_HEAP_SZ;

               // Where does the region start?
               page_ptr  = reinterpret_cast<uint8_t*>(global_page_table_obj());

               // TODO: get our current entry in the page table, and set all pages
               // allocated to PROT_NONE

               // Lock until sync process is done
               mutex_lock(&pt_lock);

               // Zero out local page table
               memset(page_ptr, 0, region_sz);

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
               // Signal unicast speaker there is queued work
               cond_signal(&uni_speaker_cond);

               MS_STATE = HEARTBEAT;
               break;

            case UNICAST_JOIN_ACCEPT_ACK_F:
               fprintf(stderr, "> received join accept ack\n");

               // Respond to the other server's listener
               retaddr.sin_port = htons(UNICAST_PORT);

               // How big is the region we're sync'ing?
               region_sz = PAGE_TABLE_OBJ_SZ
                  + PAGE_TABLE_SZ
                  + PAGE_TABLE_ALLOC_HEAP_SZ
                  + PAGE_TABLE_HEAP_SZ;

               // Where does the region start?
               page_ptr  = reinterpret_cast<uint8_t*>(global_page_table_obj());

               // TODO: lock page while we're sending it

               // Queue work to send page table
               for (i = 0; i < region_sz; i += PAGE_SZ) {

                  page_addr = reinterpret_cast<uint32_t>(page_ptr + i);
                  page_data = reinterpret_cast<void*>(page_ptr + i);

                  // If this page has non-zero contents
                  if (!isPageZeros(page_data)) {

                     work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
                     packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

                     // Push work onto the uni_speaker's queue
                     safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                           // A new work tuple
                           new (work_memory) WorkTupleType(retaddr,
                              // A new packet
                              new (packet_memory) SyncPage(page_addr, page_data))
                           );
                     // Signal unicast speaker there is queued work
                     cond_signal(&uni_speaker_cond);
                  }
               }

               work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
               packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

               // Push work onto the uni_speaker's queue
               safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                     // A new work tuple
                     new (work_memory) WorkTupleType(retaddr,
                        // A new packet
                        new (packet_memory) GenericPacket(SYNC_DONE_F))
                     );

               // Signal unicast speaker there is queued work
               cond_signal(&uni_speaker_cond);


               break;

            case SYNC_PAGE_F:
               syncp = reinterpret_cast<SyncPage*>(buffer);

               fprintf(stderr, "> received sync page (%p)\n", (void*) ntohl(syncp->page_offset));
               // Sync the page (assume page table is already locked)
               memcpy((void*) ntohl(syncp->page_offset), syncp->get_payload_ptr(), PAGE_SZ);

               break;

            case SYNC_DONE_F:
               fprintf(stderr, "> sync done\n");

               print_page_table();

               // Map any new pages and set permissions
               reserve_pages();

               // Page table can now be accessed/modified by other worker threads
               mutex_unlock(&pt_lock);

               fprintf(stderr, "> application ready!\n");
               break;

            case THREAD_CREATE_F:
               tc = reinterpret_cast<ThreadCreate*>(buffer);
               fprintf(stderr, "> thread create (func=%p)\n", (void*) ntohl(tc->func_ptr));

               // Get address of function
               func = (void*) ntohl(tc->func_ptr);
               arg  = (void*) ntohl(tc->arg);

               // Allocate a stack for the thread in the application heap
               thr_stack     = malloc(4096 * 8);
               thr_stack_ptr = (void*) ((uint8_t*) thr_stack + 4096 * 8);

               // Create the thread
               if((thr_id =
                        clone((int (*)(void*)) func,
                           thr_stack_ptr,
                           CLONE_ATTRS,
                           arg)) == -1) {
                  perror("app-thread creation failed");
                  exit(EXIT_FAILURE);
               }
               // Send the thread id and our uuid back to master
               fprintf(stderr, "> app-thread (%p) id: %d\n", thr_stack_ptr, thr_id);

               break;

            case ACQUIRE_WRITE_LOCK_F:
               // Lock the pt_owner_lock, this ensures that we are not currently inserting into the pt
               // when someone else wants to aquire the lock
               fprintf(stderr, " > Recieved a request to acquire ownership of the pt\n");
               mutex_lock(&pt_owner_lock);
               // IF we are the pt_owner
               if(pt_owner == local_addr.s_addr) {

                  // OK to give ownership of the pt away
                  pt_owner =  retaddr.sin_addr.s_addr;

                  work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
                  packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

                  fprintf(stderr, " > Release ownership of the page table\n");
                  // Inform the sender that it now has ownership of the pt
                  safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                        // A new work tuple
                        new (work_memory) WorkTupleType(retaddr,
                           // A new packet
                           new (packet_memory) ReleaseWriteLock())
                        );

                  // Signal unicast speaker there is queued work
                  cond_signal(&uni_speaker_cond);

                  // TODO: BROADCAST NEW PT OWNER
               }
               else {
                  fprintf(stderr," > Need to reroute\n");
                  //TODO: send the reroute packet, we are not the pt_owner
               }

               mutex_unlock(&pt_owner_lock);
               break;

            case RELEASE_WRITE_LOCK_F:
               // The lock is always released to a client/server connection (never to the
               // global listeners).  See mmapwrapper.h for an example.
               fprintf(stderr, "ERROR> global listener heard a RELEASE_WRITE_LOCK_F\n");
               break;

            default:
               fprintf(stderr, "ERROR> unknown packet type (%x)\n", p->get_flag());
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
                     // Give ourselves write lock on page table
                     pt_owner = local_addr.s_addr;
                     fprintf(stderr, " > Giving ourselves ownership, pt_owner = %s\n", inet_ntoa((struct in_addr&)pt_owner));
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
            Packet*        p    = NULL;
            WorkTupleType* work = NULL;

            switch (MS_STATE) {

            case JOIN:
               //TODO: use binary form of IP and ditch the string payload
               // Build packet
               p = new (buffer) MulticastJoin(strlen(local_ip), &main, &_end, (uint8_t*) global_base());
               // Add packet payload (the user IP address)
               memcpy(p->get_payload_ptr(), local_ip, strlen(local_ip));
               break;

            case HEARTBEAT:
               //TODO: use binary form of IP and ditch the string payload
               // Build packet
               p = new (buffer) MulticastHeartbeat(strlen(local_ip));
               // Add packet payload (the user IP address)
               memcpy(p->get_payload_ptr(), local_ip, strlen(local_ip));
               break;

            }

            // Check if there is work on the deque
            if (safe_size(&multi_speaker_work_deque, &multi_speaker_lock) > 0) {
               // Pop work from work queue
               work = safe_pop(&multi_speaker_work_deque, &multi_speaker_lock);
            }

            // If there is work, override default action
            if (work != NULL) {
               p = work->second;
            }

            //fprintf(stderr, "> broadcasting a packet (0x%x)!\n", p->get_flag());

            // Send whatever we just built
            if (sendto(sk, p, buffer_sz, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
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
               // Ignore multicast messages from ourself
               if(local_addr.s_addr != addr.sin_addr.s_addr) {
                  // Send the buffer off for processing
                  multi_listener_event_loop(packet_buffer, nbytes);
               }
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
             */
            Packet*                p              = NULL;
            MulticastJoin*         mjp            = NULL;
            MulticastHeartbeat*    mjh            = NULL;
            UnicastJoinAcceptance* uja            = NULL;
            SyncReserve*           sr             = NULL;
            WorkTupleType*         work           = NULL;
            void*                  packet_memory  = NULL;
            void*                  work_memory    = NULL;
            void*                  test           = NULL;
            char*                  payload_buf    = NULL;
            uint32_t               payload_sz     = 0;
            uint32_t               ip             = 0;
            uint32_t               start_addr     = 0;
            uint32_t               memory_sz      = 0;
            struct sockaddr_in     retaddr        = {0};
            struct in_addr         addr           = {0};

            // Generic packet data (type/payload size/payload)
            p           = reinterpret_cast<Packet*>(buffer);
            payload_sz  = p->get_payload_sz();
            payload_buf = reinterpret_cast<char*>(p->get_payload_ptr());

            //fprintf(stderr, "> heard packet (%x)\n", p->get_flag());

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

                     ip = inet_addr(payload_buf);

                     // Esnure user is not in page table
                     // Debug
                     fprintf(stderr, "> Adding %s to page_table\n", payload_buf);

                     page_table->insert(
                           // IP -> std::vector<Page>
                           std::pair<uint32_t, PageVectorType*>(
                              ip,
                              new (get_pt_heap(&pt_lock)->malloc(sizeof(PageVectorType))) PageVectorType()));

                     // Allocate memory for the new work/packet
                     work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
                     packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

                     // Who to contact
                     retaddr.sin_family      = AF_INET;
                     retaddr.sin_addr.s_addr = inet_addr(payload_buf);
                     retaddr.sin_port        = htons(UNICAST_PORT);

                     //TODO: use binary form of IP and ditch the string payload
                     // Push work onto the uni_speaker's queue
                     safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                           // A new work tuple
                           new (work_memory) WorkTupleType(retaddr,
                              // A new packet
                              new (packet_memory) UnicastJoinAcceptance(strlen(local_ip),
                                 _start_page_table,
                                 _end_page_table,
                                 _next_uuid++,
                                 pt_owner))
                           );
                     // Signal unicast speaker there is queued work
                     cond_signal(&uni_speaker_cond);
                  }
                  else {
                     fprintf(stderr, "> invalid address space detected\n");
                  }
               }
               break;

            case SYNC_RESERVE_F:
               sr = reinterpret_cast<SyncReserve*>(buffer);

               ip             = ntohl(sr->ip);
               start_addr     = ntohl(sr->start_addr);
               memory_sz      = ntohl(sr->sz);

               // If message is from someone in page table
               if (page_table->count(ip) > 0) {
                  // Convert to in_addr struct
                  addr.s_addr = ip;

                  fprintf(stderr, "> %s reserving %p(%d)\n",
                        inet_ntoa(addr),
                        (void*) start_addr,
                        memory_sz);

                  // Map this memory into our address space
                  test = mmap((void*) start_addr, 
                        memory_sz, 
                        PROT_NONE,
                        MAP_FIXED | MAP_ANON, -1, 0);

                  if (test == MAP_FAILED) {
                     fprintf(stderr, "> map failed\n");
                  }

                  // TODO: insert this mapping into the page table (i think)

               }
               else {
                  fprintf(stderr, "> Reserve request from non-cluster member\n");
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

      PageTableItr    pt_itr;
      PageVectorItr   vec_itr;
      PageVectorType* temp   = NULL;
      struct in_addr  addr   = {0};
      int found = 0;

      fprintf(stderr, "**** Searching through page table ****\n");
      for (pt_itr = page_table->begin(); !found && pt_itr != page_table->end(); pt_itr++) {
         int found = 0;
         addr.s_addr = (*pt_itr).first;
         fprintf(stderr, "%% %s : <\n", inet_ntoa(addr));
         temp = (*pt_itr).second;
         for (vec_itr = temp->begin(); !found && vec_itr != temp->end(); vec_itr++) {
            // IF the faulting address is on this machine
            if(p == (void*) (*vec_itr)->address) {
               fprintf(stderr,"Found!");
               found = 1;
            }
         }
         // If the packet wasn't found
         if(!found) {
            fprintf(stderr,"Not found:[");
         }
         fprintf(stderr, ">\n");
      }
      fprintf(stderr, "********************\n\n");

      // TODO: Add in logic to pull the page table from the network and set permissions

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


   void nlcbsmm_init() {

      int base;

      asm("\t movl %%esp,%0" : "=r"(base));

      fprintf(stderr, ">> stack base = %p\n", (void*) base);

      /**
       * Hook entry.
       */
      void* raw_obj    = (void*) mmap((void*) global_page_table_obj(),
            PAGE_TABLE_OBJ_SZ,
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
            -1, 0);
      if (raw_obj == MAP_FAILED) {
         fprintf(stderr, "Cannot map gpto: %p\n", (void*) global_page_table_obj());
      }
      pt_heap = new (raw_obj) PageTableHeapType();

      // Ensure that the pt_heap region is being mmaped
      pt_heap->free(pt_heap->malloc(8));
      // Ensure that the clone_heap region is being mmaped
      clone_heap.free(clone_heap.malloc(8));

      // Dedicated memory to maintaining the page table
      void* raw         = (void*) mmap((void*) global_page_table(),
            PAGE_TABLE_SZ,
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
            -1, 0);
      if (raw_obj == MAP_FAILED) {
         fprintf(stderr, "Cannot map gpt: %p\n", (void*) global_page_table());
      }
      page_table        = new (raw) PageTableType();
      _start_page_table = (uint32_t) raw;
      _end_page_table   = (uint32_t) ((uint8_t*) raw) + PAGE_TABLE_SZ;
      _uuid             = (uint32_t) -1;

      // Obtain the IP address of the local ethernet interface
      local_ip = get_local_interface();
      // Binary form of IP address
      local_addr.s_addr = inet_addr(local_ip);

      // Setup condition and mutex variables
      cond_init(&uni_speaker_cond,       NULL);
      mutex_init(&uni_speaker_cond_lock, NULL);
      mutex_init(&uni_speaker_lock,      NULL);
      mutex_init(&multi_speaker_lock,    NULL);
      mutex_init(&pt_owner_lock,         NULL);
      mutex_init(&pt_lock,               NULL);

      pt_owner = -1;

      print_init_message();

      // Register SIGSEGV handler
      //register_signal_handlers();

      // Spawn the thread that speaks/listens to cluster
      networkmanager.start_comms();
   }

}
