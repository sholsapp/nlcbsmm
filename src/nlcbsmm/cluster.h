#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#include "vmmanager.h"

namespace NLCBSMM {

   class ClusterCoordinator {

      public:
         ClusterCoordinator() {
            /**
             * Constructor
             */
            multi_speaker_thread_id  = 0;
            multi_listener_thread_id = 0;
            uni_speaker_thread_id    = 0;
            uni_listener_thread_id   = 0;
         }


         ~ClusterCoordinator(){
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


         static void zero_pt() {
            /**
             *
             */
            uint32_t region_sz = 0;
            void*    page_ptr  = NULL;

            // How big is the region we're sync'ing?
            region_sz = PAGE_TABLE_MACH_LIST_SZ
               + PAGE_TABLE_OBJ_SZ
               + PAGE_TABLE_SZ
               + PAGE_TABLE_ALLOC_HEAP_SZ
               + PAGE_TABLE_HEAP_SZ;

            // Where does the region start?
            page_ptr  = reinterpret_cast<uint8_t*>(global_pt_start_addr());

            if(local_addr.s_addr != pt_owner) {
               // Zero out local page table
               memset(page_ptr, 0, region_sz);
            }
            else {
               fprintf(stderr,"WARNING!!!!!!!!!!!!!!!!!!!!!! PT_OWNER attempting to zero pages table.\n");
            }
            return;
         }


         void start_workers() {
            /**
             *
             */
            void* argument           = NULL;
            void* worker0_ptr        = NULL;
            void* worker1_ptr        = NULL;
            void* worker2_ptr        = NULL;
            void* worker3_ptr        = NULL;
            void* worker0_stack      = NULL;
            void* worker1_stack      = NULL;
            void* worker2_stack      = NULL;
            void* worker3_stack      = NULL;
            void* worker0_fixed_addr = NULL;
            void* worker1_fixed_addr = NULL;
            void* worker2_fixed_addr = NULL;
            void* worker3_fixed_addr = NULL;

            fprintf(stderr, "Starting workers...\n");

            worker0_fixed_addr = ((uint8_t*) get_workers_fixed_addr()) + (WORKER_STACK_SZ * 0);

            worker0_stack = (void*) mmap(worker0_fixed_addr,
                  WORKER_STACK_SZ,
                  CLONE_MMAP_PROT_FLAGS,
                  CLONE_MMAP_FLAGS, -1, 0);

            if (worker0_stack == MAP_FAILED)
               perror("!> mmap failed to allocate worker stack");

            worker0_ptr      = (void*) (((uint8_t*) worker0_stack)    + WORKER_STACK_SZ);

            if((worker0_thread_id =
                     clone(&worker_func,
                        worker0_ptr,
                        CLONE_ATTRS,
                        argument)) == -1) {
               perror("vmmanager.cpp, clone, worker0");
               exit(EXIT_FAILURE);
            }

            return;
         }


         static int worker_func(void* arg) {
            /**
             * Workers perform work that may block (wait for networked pthread_join) when it
             * is unacceptable to block the main speakers/listeners.
             */
            ThreadWorkType* work                = NULL;
            PthreadWork*    pthread_work        = NULL;
            Packet*         p                   = NULL;
            GenericPacket*  g                   = NULL;
            void*           packet_memory       = NULL;
            uint32_t        sk                  =  0;
            uint32_t        thr_id              =  0;
            uint32_t        thr_ret             =  0;
            uint32_t        addrlen             =  0;
            uint32_t        selflen             =  0;
            uint32_t        timeout             =  0;
            struct          sockaddr_in self    = {0};
            struct          sockaddr_in retaddr = {0};

            selflen = sizeof(struct sockaddr_in);
            addrlen = sizeof(struct sockaddr_in);

            while(1) {

               // If there is no work in the queue
               if (safe_thread_size(&thread_deque, &thread_deque_lock) == 0) {
                  // Wait for work (blocks until signal from other thread)
                  cond_wait(&thread_cond, &thread_cond_lock);
                  // The cond_wait specifies that it returns with second param in a locked state
                  mutex_unlock(&thread_cond_lock);
               }

               // Pop work from work queue
               work = safe_thread_pop(&thread_deque, &thread_deque_lock);

               if (work != NULL) {

                  fprintf(stderr, "> worker func running\n");

                  // Set up client/server
                  sk = new_comm();
                  getsockname(sk, (struct sockaddr*) &self, &selflen);

                  // Get work info
                  retaddr      = work->first;
                  pthread_work = &work->second;

                  // Start thread, get thread_id
                  if((thr_id =
                           clone((int (*)(void*)) pthread_work->func,
                              (void*) pthread_work->stack_ptr,
                              CLONE_ATTRS,
                              (void*) pthread_work->arg)) == -1) {
                     perror("app-thread creation failed");
                     exit(EXIT_FAILURE);
                  }

                  fprintf(stderr, "> app-thread (%p) id: %d\n",
                        (void*) pthread_work->stack_ptr,
                        thr_id);

                  // Don't timeout
                  timeout = 10000000;

                  // Send thread_id to caller, wait for ThreadJoin
                  packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
                  p = persistent_blocking_comm(sk, (struct sockaddr*) &retaddr,
                        new (packet_memory) ThreadCreateAck(thr_id),
                        timeout, "worker func - thread create ack");

                  fprintf(stderr, "> thread worker got packet!\n");

                  // Check if received a ThreadJoin
                  if (p->get_flag() == THREAD_JOIN_F) {
                     fprintf(stderr, " > Wait for thread %d\n", thr_id);

                     // Wait for thr_id
                     if((thr_ret = waitpid(thr_id, NULL,  __WCLONE)) == -1) {
                        perror("wait error");
                     }

                     fprintf(stderr, "> Thread waited for\n");

                     // Sync pages with retaddr

                     // Indicate finished
                     direct_comm(retaddr,
                           new (clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ))
                           GenericPacket(THREAD_JOIN_ACK_F));

                  }

                  clone_heap.free(p);
               }

            }

            return 0;
         }


         void start_arbiters() {
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

            uni_listener_fixed_addr   = ((uint8_t*) get_clonebase_fixed_addr()) + (CLONE_STACK_SZ * 0);
            uni_speaker_fixed_addr    = ((uint8_t*) get_clonebase_fixed_addr()) + (CLONE_STACK_SZ * 1);
            multi_listener_fixed_addr = ((uint8_t*) get_clonebase_fixed_addr()) + (CLONE_STACK_SZ * 2);
            multi_speaker_fixed_addr  = ((uint8_t*) get_clonebase_fixed_addr()) + (CLONE_STACK_SZ * 3);

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

            if((uni_listener_thread_id =
                     clone(&uni_listener,
                        uni_listener_ptr,
                        CLONE_ATTRS,
                        argument)) == -1) {
               perror("vmmanager.cpp, clone, listener");
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


            if((multi_listener_thread_id =
                     clone(&multi_listener,
                        multi_listener_ptr,
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


         }


         void start_comms() {
            /**
             *
             */
            start_workers();
            start_arbiters();
            return;
         }


         static int uni_speaker(void* t) {
            /**
             *
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

               //fprintf(stderr, "> unicast waiting for work\n");

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

                  //fprintf(stderr, "> sending a packet (0x%x) to %s:%d!\n", p->get_flag(), inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

                  if (sendto(sk, p, MAX_PACKET_SZ, 0, (struct sockaddr *) &addr , sizeof(addr)) < 0) {
                     perror("cluster.h, 1, sendto");
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
            ThreadCreateAck*       tca            = NULL;
            AcquireWriteLock*      awl            = NULL;
            AcquirePage*           ap             = NULL;
            ReleasePage*           rp             = NULL;
            SyncReroute*           reroute_pack   = NULL;
            WorkTupleType*         work           = NULL;

            uint8_t*               payload_buf    = NULL;
            uint8_t*               page_ptr       = NULL;
            uint8_t*               page_aligned   = NULL;
            void*                  packet_memory  = NULL;
            void*                  work_memory    = NULL;
            void*                  thread_work_memory = NULL;
            void*                  page_data      = NULL;
            void*                  thr_stack      = NULL;
            void*                  thr_stack_ptr  = NULL;
            void*                  func           = NULL;
            void*                  arg            = NULL;
            void*                  test           = NULL;
            uint32_t               i              = 0;
            uint32_t               current_owner  = 0;
            uint32_t               region_sz      = 0;
            uint32_t               payload_sz     = 0;
            uint32_t               page_addr      = 0;
            uint32_t               thr_id         = 0;
            uint32_t               thr_stack_sz   = 0;

            Machine*               node           = NULL;
            Page*                  page           = NULL;


            // Generic packet data (type/payload size/payload)
            p           = reinterpret_cast<Packet*>(buffer);
            payload_sz  = p->get_payload_sz();
            payload_buf = reinterpret_cast<uint8_t*>(p->get_payload_ptr());

            //fprintf(stderr, "> received a packet (%x)\n", p->get_flag());

            switch (p->get_flag()) {

            case UNICAST_JOIN_ACCEPT_F:
               //fprintf(stderr, "> received join accept\n");

               uja = reinterpret_cast<UnicastJoinAcceptance*>(buffer);

               // record our uuid from the master
               _uuid = ntohl(uja->uuid);
               // record the current pt_owner
               pt_owner = ntohl(uja->pt_owner);

               // TODO: get our current entry in the page table, and set all pages
               // allocated to PROT_NONE

               // Lock until sync process is done
               mutex_lock(&pt_lock);

               // Zero out local page table
               zero_pt();

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

               break;

            case UNICAST_JOIN_ACCEPT_ACK_F:
               //fprintf(stderr, "> received join accept ack\n");

               // Passively sync the page table region
               passive_pt_sync(retaddr);

               // TODO: error checking
               node_list->find(retaddr.sin_addr.s_addr)->second->status = MACHINE_IDLE;
               break;


            case SYNC_ACQUIRE_PAGE_F:
               ap = reinterpret_cast<AcquirePage*>(buffer);
               page_addr     = ntohl(ap->page_addr);

               current_owner = get_owner(page_addr);

               packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
               rp            = new (packet_memory) ReleasePage(page_addr);
               // TODO: this needs to wait for ack!
               //direct_comm(retaddr, rp);
               p = blocking_comm((struct sockaddr*) &retaddr, rp, 5, "release page ack");
               if (p->get_flag() != SYNC_RELEASE_PAGE_ACK_F)
                  fprintf(stderr, "> Bad sync release page ack (%x)!\n", p->get_flag());

               // Set page table ownership/permissions
               set_new_owner(page_addr, retaddr.sin_addr.s_addr);

               if(mprotect((void*) page_addr, PAGE_SZ, PROT_NONE) == -1) {
                  fprintf(stderr, "ERROR> MPROTECT FAILED, on page %p\n", (void*)page_addr);
               }

               break;

            case SYNC_START_F:
               mutex_lock(&pt_lock);
               zero_pt();
               work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
               packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
               safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                     new (work_memory) WorkTupleType(retaddr,
                        new (packet_memory) GenericPacket(SYNC_START_ACK_F))
                     );
               cond_signal(&uni_speaker_cond);
               break;

            case SYNC_PAGE_F:
               syncp = reinterpret_cast<SyncPage*>(buffer);
               fprintf(stderr, "> received sync page (%p)\n", (void*) ntohl(syncp->page_offset));
               // Sync the page (assume page table is already locked)
               memcpy((void*) ntohl(syncp->page_offset),
                     syncp->get_payload_ptr(),
                     PAGE_SZ);
               //safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
               //      new (work_memory) WorkTupleType(retaddr,
               //         new (packet_memory) GenericPacket(SYNC_PAGE_ACK_F))
               //      );
               //cond_signal(&uni_speaker_cond);
               direct_comm(retaddr,
                     new (clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ))
                     GenericPacket(SYNC_PAGE_ACK_F));
               break;

            case SYNC_DONE_F:
               //fprintf(stderr, "> sync done\n");

               // TODO: error checking
               node_list->find(local_addr.s_addr)->second->status = MACHINE_IDLE;

               // Map any new pages and set permissions
               reserve_pages();

               // TODO: need better assurance that sync is actually done (have we already
               // received and mapped all the pages?).
               work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
               packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
               safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                     new (work_memory) WorkTupleType(retaddr,
                        new (packet_memory) GenericPacket(SYNC_DONE_ACK_F))
                     );
               cond_signal(&uni_speaker_cond);

               // Page table can now be accessed/modified by other worker threads
               mutex_unlock(&pt_lock);

               fprintf(stderr, "> application ready!\n");
               break;

            case THREAD_CREATE_F:
               tc = reinterpret_cast<ThreadCreate*>(buffer);
               fprintf(stderr, "%lld > thread create (func=%p)\n", get_micro_clock(), (void*) ntohl(tc->func_ptr));

               // Received work, we're now active
               node_list->find(local_addr.s_addr)->second->status = MACHINE_ACTIVE;

               // Get where caller put thread stack
               // Use padding convention to get a thread-stack-sized page-aligned section of memory
               fprintf(stderr, "> Parsing thread create packet...");
               // Page aligned ptr
               page_aligned  = (uint8_t*) page_align((void*) ntohl(tc->stack_ptr));
               // Wastes some memory
               thr_stack     = (void*) (page_aligned + PAGE_SZ);
               // Discard the padding for actual stack size
               thr_stack_sz  = ntohl(tc->stack_sz);
               thr_stack_ptr = (void*) ((uint8_t*) thr_stack + thr_stack_sz);
               fprintf(stderr, "done\n");

               fprintf(stderr, "> Mapping pthread stack memory\n");
               // Map this memory into our address space
               if((test = mmap((void*) thr_stack,
                           thr_stack_sz,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
                           -1, 0)) == MAP_FAILED) {
                  fprintf(stderr, "> map failed\n");
               }
               else {
                  fprintf(stderr, "> mapped pthread stack at %p\n", thr_stack);
               }

               // Get address of function
               func = (void*) ntohl(tc->func_ptr);
               arg  = (void*) ntohl(tc->arg);

               fprintf(stderr, "> queue work for worker_func\n");

               // Queue pthread work
               thread_work_memory = clone_heap.malloc(sizeof(ThreadWorkType));
               safe_thread_push(&thread_deque, &thread_deque_lock,
                     new (thread_work_memory) ThreadWorkType(retaddr,
                        PthreadWork((uint32_t) func,
                           (uint32_t) arg,
                           (uint32_t) thr_stack_ptr)));
               // Signal unicast speaker there is queued work
               cond_signal(&thread_cond);

               break;

            case ACQUIRE_WRITE_LOCK_F:
               fprintf(stderr, " > Recieved a request to acquire ownership of the pt\n");
               awl = reinterpret_cast<AcquireWriteLock*>(buffer);
               mutex_lock(&pt_owner_lock);
               // IF we are the pt_owner
               if(pt_owner == local_addr.s_addr) {

                  fprintf(stderr, "> Active sync to %s:%d\n",
                        inet_ntoa((struct in_addr&) retaddr.sin_addr),
                        ntohs(awl->ret_port));

                  // Respond to the specified sync port (already in network order)
                  retaddr.sin_port = awl->ret_port;

                  // Sync page table region (locks pt_lock)
                  active_pt_sync(retaddr);

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
                           new (packet_memory) ReleaseWriteLock(next_addr))
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
            case SYNC_RELEASE_PAGE_F:
            case SYNC_DONE_ACK_F:
               // These types of packets are handled directly
               fprintf(stderr, "ERROR> ignored packet type(%x)\n", p->get_flag());
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

            void*               buffer = NULL;
            size_t              psz    = 0;
            uint32_t            sk     = 0;
            uint32_t            cnt    = 0;
            struct ip_mreq      mreq   = {0};
            struct sockaddr_in  addr   = {0};
            Packet*             p      = NULL;
            WorkTupleType*      work   = NULL;

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("vmmanager.cpp, socket");
               exit(EXIT_FAILURE);
            }

            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = inet_addr(MULTICAST_GRP);
            addr.sin_port        = htons(MULTICAST_PORT);

            // This buffer is big enough to hold the biggest packet
            psz     = MAX_PACKET_SZ;
            buffer  = clone_heap.malloc(psz);

            // Build packet
            p = new (buffer) MulticastJoin(local_addr.s_addr,
                  (uint8_t*) global_main(),
                  (uint8_t*) global_end(),
                  (uint8_t*) global_base());

            for (cnt = 0; cnt < MAX_JOIN_ATTEMPTS && _uuid == -1; cnt++) {
               // Send join request
               if (sendto(sk, p, psz, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                  perror("cluster.h, join, sendto");
                  exit(EXIT_FAILURE);
               }
               sleep(1);
            }

            // No one responded
            if (_uuid == -1) {
               // I am master
               _uuid = 0;

               // Set status to master
               node_list->find(local_addr.s_addr)->second->status = MACHINE_MASTER;

               fprintf(stdout, " > Master ready\n",
                     inet_ntoa((struct in_addr&)pt_owner));

               // Give ourselves write lock on page table
               pt_owner = local_addr.s_addr;

            }

            for (cnt = 0; ; cnt++) {
               // Clear the memory buffer each time
               memset(buffer, 0, psz);

               //TODO: use binary form of IP and ditch the string payload
               // Build packet
               p = new (buffer) MulticastHeartbeat(strlen(local_ip));
               // Add packet payload (the user IP address)
               memcpy(p->get_payload_ptr(), local_ip, strlen(local_ip));

               // Check if there is work on the deque
               if (safe_size(&multi_speaker_work_deque, &multi_speaker_lock) > 0) {
                  // Pop work from work queue
                  work = safe_pop(&multi_speaker_work_deque, &multi_speaker_lock);

                  // If there is work, override default action
                  if (work != NULL) {
                     // Overwrite the packet with useful work
                     memcpy(buffer, work->second, MAX_PACKET_SZ);
                     clone_heap.free(work->second);
                  }

               }

               //fprintf(stderr, "> broadcasting a packet (0x%x)!\n", p->get_flag());

               // Send whatever we just built
               if (sendto(sk, p, psz, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                  perror("cluster.h, speaker, sendto");
                  exit(EXIT_FAILURE);
               }

               // TODO: replace sleeping with checking the work queue for work?
               sleep(1);
            }

            // Shit is scarce, son!
            clone_heap.free(buffer);
            return 0;
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
            void*                  raw            = NULL;
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

                  // Verify request's address space requirements
                  if ((uint32_t) global_main() == ntohl(mjp->main_addr)
                        && (uint32_t) global_end() == ntohl(mjp->end_addr)
                        && (uint32_t) global_base() == ntohl(mjp->prog_break_addr)) {

                     addr.s_addr = ip = ntohl(mjp->ip_address);

                     // TODO: make sure user isn't is in the page table

                     //fprintf(stderr, "> Adding %s to node list\n", inet_ntoa(addr));
                     raw = get_pt_heap(&pt_lock)->malloc(sizeof(Machine));
                     node_list->insert(
                           std::pair<uint32_t, Machine*>(
                              ip,
                              new (raw) Machine(ip))
                           );

                     // Allocate memory for the new work/packet
                     work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
                     packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

                     // Who to contact
                     retaddr.sin_family      = AF_INET;
                     retaddr.sin_addr.s_addr = ip;
                     retaddr.sin_port        = htons(UNICAST_PORT);

                     //TODO: use binary form of IP and ditch the string payload
                     // Push work onto the uni_speaker's queue
                     safe_push(&uni_speaker_work_deque, &uni_speaker_lock,
                           // A new work tuple
                           new (work_memory) WorkTupleType(retaddr,
                              // A new packet
                              new (packet_memory) UnicastJoinAcceptance(local_addr.s_addr,
                                 _start_page_table,
                                 _end_page_table,
                                 _next_uuid++,
                                 pt_owner))
                           );
                     // Signal unicast speaker there is queued work
                     cond_signal(&uni_speaker_cond);
                  }
                  else {
                     // TODO: error-handling
                     fprintf(stderr, "> invalid address space detected\n");
                  }
               }
               break;

            case SYNC_RESERVE_F:
               /*
                  sr = reinterpret_cast<SyncReserve*>(buffer);

                  ip             = ntohl(sr->ip);
                  start_addr     = ntohl(sr->start_addr);
                  memory_sz      = ntohl(sr->sz);

               // Convert to in_addr struct
               addr.s_addr = ip;

               // If message is from someone in page table
               if (node_list->count(ip) > 0
               // And we didn't send this packet
               && ip != local_addr.s_addr) {

               fprintf(stderr, "> %s reserving %p(%d)\n",
               inet_ntoa(addr),
               (void*) start_addr,
               memory_sz);

               // Map this memory into our address space
               if((test = mmap((void*) start_addr,
               memory_sz,
               PROT_NONE,
               MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
               -1, 0)) == MAP_FAILED) {
               fprintf(stderr, "> map failed, setting protections\n");
               mprotect((void*) start_addr, memory_sz, PROT_NONE);
               }
               else {
               fprintf(stderr, "> Reserved %p (%d) for %s\n",
               (void*) start_addr,
               memory_sz,
               inet_ntoa(addr));
               }
               // TODO: insert this mapping into the page table (i think)
               }
               else {
               fprintf(stderr, "> Reserve request from non-cluster member %s\n", inet_ntoa(addr));
               }
                */
               break;

            case MULTICAST_HEARTBEAT_F:
               mjh = reinterpret_cast<MulticastHeartbeat*>(buffer);
               //fprintf(stderr, "%s: <3\n", payload_buf);
               break;

            default:
               break;

            }
         }


         static void active_pt_sync(struct sockaddr_in retaddr) {
            /**
             * TODO: implement this
             */
            Packet*                p              = NULL;
            GenericPacket*         gp             = NULL;
            SyncPage*              syncp          = NULL;
            WorkTupleType*         work           = NULL;

            uint8_t*               page_ptr       = NULL;
            void*                  packet_memory  = NULL;
            void*                  work_memory    = NULL;
            void*                  page_data      = NULL;
            uint32_t               region_sz      = 0;
            uint32_t               page_addr      = 0;
            uint32_t               i              = 0;
            uint32_t               timeout        = 0;

            struct sockaddr_in     addr           = {0};

            // Set timeout to 5 seconds
            timeout = 5;

            // Respond to the other server's listener
            retaddr.sin_port = htons(UNICAST_PORT);

            // Don't want to modify retaddr in-place
            addr = retaddr;

            // How big is the region we're sync'ing?
            region_sz = PAGE_TABLE_MACH_LIST_SZ
               + PAGE_TABLE_OBJ_SZ
               + PAGE_TABLE_SZ
               + PAGE_TABLE_ALLOC_HEAP_SZ
               + PAGE_TABLE_HEAP_SZ;

            // Where does the region start?
            page_ptr  = reinterpret_cast<uint8_t*>(global_pt_start_addr());

            packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
            // Send a SYNC_DONE_F and wait for ack
            p = blocking_comm(
                  (struct sockaddr*) &addr,
                  new (packet_memory) GenericPacket(SYNC_START_F),
                  timeout,
                  "sync start");

            if (p) {
               if (p->get_flag() != SYNC_START_ACK_F)
                  fprintf(stderr, "> Bad sync start ack (%x)!\n", p->get_flag());

               // TODO: memory leak
               //clone_heap.free(p);
            }
            else {
               fprintf(stderr, "> Sync start null response!\n");
            }

            for (i = 0; i < region_sz; i += PAGE_SZ) {

               page_addr = reinterpret_cast<uint32_t>(page_ptr + i);
               page_data = reinterpret_cast<void*>(page_ptr + i);

               // If this page has non-zero contents
               if (!isPageZeros(page_data)) {

                  packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

                  // TODO: make this persistent to avoid socket create/destroy per packet
                  addr = retaddr; // no modify in place
                  p = blocking_comm(
                        (struct sockaddr*) &addr,
                        new (packet_memory) SyncPage(page_addr, page_data),
                        15,
                        "sync page");

                  if (p->get_flag() != SYNC_PAGE_ACK_F)
                     fprintf(stderr, "> Bad sync page ack (%x)!\n", p->get_flag());

                  // TODO: memory leak
                  clone_heap.free(p);
               }
            }

            packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
            // Send a SYNC_DONE_F and wait for ack
            addr = retaddr; // no modify in place
            p = blocking_comm(
                  (struct sockaddr*) &retaddr,
                  new (packet_memory) GenericPacket(SYNC_DONE_F),
                  timeout,
                  "sync done");

            if (p->get_flag() != SYNC_DONE_ACK_F)
               fprintf(stderr, "> Bad sync done ack (%x)!\n", p->get_flag());

            // TODO: memory leak
            //clone_heap.free(p);

            return;
         }


         static void passive_pt_sync(struct sockaddr_in retaddr) {
            /**
             *
             */
            Packet*                p              = NULL;
            GenericPacket*         gp             = NULL;
            SyncPage*              syncp          = NULL;
            WorkTupleType*         work           = NULL;

            uint8_t*               page_ptr       = NULL;
            void*                  packet_memory  = NULL;
            void*                  work_memory    = NULL;
            void*                  page_data      = NULL;
            uint32_t               region_sz      = 0;
            uint32_t               page_addr      = 0;
            uint32_t               i              = 0;

            // Respond to the other server's listener
            retaddr.sin_port = htons(UNICAST_PORT);

            // How big is the region we're sync'ing?
            region_sz = PAGE_TABLE_MACH_LIST_SZ
               + PAGE_TABLE_OBJ_SZ
               + PAGE_TABLE_SZ
               + PAGE_TABLE_ALLOC_HEAP_SZ
               + PAGE_TABLE_HEAP_SZ;

            // Where does the region start?
            page_ptr  = reinterpret_cast<uint8_t*>(global_pt_start_addr());

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

            return;
         }


         static uint32_t new_comm(uint32_t port=0, bool print=false) {
            /**
             * Takes about 6 microseconds
             */
            uint32_t sk               =  0;
            uint32_t selflen          =  0;
            struct   sockaddr_in self = {0};

            uint64_t start, end;

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("cluster.h, 1, socket");
               exit(EXIT_FAILURE);
            }

            self.sin_family      = AF_INET;
            self.sin_port        = htons(port); // Any
            selflen              = sizeof(self);

            if (bind(sk, (struct sockaddr *) &self, selflen) < 0) {
               perror("cluster.h, bind");
               exit(EXIT_FAILURE);
            }

            getsockname(sk, (struct sockaddr*) &self, &selflen);

            if (print)
               fprintf(stderr, ">> new listener on %d\n", ntohs(self.sin_port));

            return sk;
         }


         static void direct_comm(struct sockaddr_in retaddr, Packet* send) {
            /**
             *
             */
            void*    ptr              = NULL;
            uint8_t* rec_buffer       = NULL;
            uint32_t sk               =  0;
            uint32_t nbytes           =  0;
            uint32_t addrlen          =  0;
            uint32_t selflen          =  0;
            uint32_t ret              =  0;
            struct   sockaddr_in self = {0};
            Packet*  p                = NULL;

            // Setup client/server to block until lock is acquired
            sk = new_comm();

            //fprintf(stderr, "> Direct communication to %s:%d\n", inet_ntoa(retaddr.sin_addr), retaddr.sin_port);

            // Send packet
            if (sendto(sk,
                     send,
                     MAX_PACKET_SZ,
                     0,
                     (struct sockaddr *) &retaddr,
                     sizeof(retaddr)) < 0) {
               perror("cluster.h, 3, sendto");
               exit(EXIT_FAILURE);
            }

            // Release memory
            clone_heap.free(send);
            // Close socket
            close(sk);
         }


         static Packet* persistent_blocking_comm(uint32_t sk, struct sockaddr* to, Packet* packet, uint32_t timeout, const char* id) {
            /**
             *
             */
            uint8_t* rec_buffer       = NULL;
            uint32_t nbytes           =  0;
            uint32_t addrlen          =  0;
            uint32_t selflen          =  0;
            uint32_t ret              =  0;

            addrlen = sizeof(struct sockaddr);

            rec_buffer = (uint8_t*) clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

            for (int c = 0; c < timeout; c++) {

               // Send packet
               if (sendto(sk,
                        packet,
                        MAX_PACKET_SZ,
                        0,
                        (struct sockaddr *) to,
                        addrlen) < 0) {
                  perror("cluster.h, 659, sendto");
                  exit(EXIT_FAILURE);
               }

               if ((ret = select_call(sk, 1, 0)) > 0) {

                  if ((nbytes = recvfrom(sk,
                              rec_buffer,
                              MAX_PACKET_SZ,
                              0,
                              (struct sockaddr*) to,
                              &addrlen)) < 0) {
                     perror("recvfrom");
                     exit(EXIT_FAILURE);
                  }
                  // Release memory (NOTE: we're returning the rec_buffer (don't free))
                  clone_heap.free(packet);
                  return reinterpret_cast<Packet*>(rec_buffer);
               }
            }
            fprintf(stderr, "> Persistent blocking comm timed out (%s)\n", id);
            // Release memory (NOTE: we're returning the rec_buffer (don't free))
            clone_heap.free(packet);
            return NULL;
         }


         static Packet* blocking_comm(struct sockaddr* addr, Packet* send, uint32_t timeout, const char* id) {
            /**
             *
             */
            void*    ptr              = NULL;
            uint8_t* rec_buffer       = NULL;
            uint32_t sk               =  0;
            uint32_t nbytes           =  0;
            uint32_t addrlen          =  0;
            uint32_t selflen          =  0;
            uint32_t ret              =  0;
            //struct   sockaddr_in addr = {0};
            struct   sockaddr_in self = {0};
            Packet*  p                = NULL;

            //sk = new_comm(0, true);
            sk = new_comm();

            //addr.sin_family      = AF_INET;
            //addr.sin_addr.s_addr = rec_ip;
            //addr.sin_port        = htons(UNICAST_PORT);
            addrlen              = sizeof(struct sockaddr);

            rec_buffer = (uint8_t*) clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

            memset(rec_buffer, 0, MAX_PACKET_SZ);

            for (int c = 0; c < timeout; c++) {

               // Send packet
               if (sendto(sk,
                        send,
                        MAX_PACKET_SZ,
                        0,
                        addr,
                        addrlen) < 0) {
                  perror("cluster.h, blocking, sendto");
                  exit(EXIT_FAILURE);
               }

               if ((ret = select_call(sk, 1, 0)) > 0) {

                  if ((nbytes = recvfrom(sk,
                              rec_buffer,
                              MAX_PACKET_SZ,
                              0,
                              addr,
                              &addrlen)) < 0) {
                     perror("recvfrom");
                     exit(EXIT_FAILURE);
                  }
                  // Release memory (NOTE: we're returning the rec_buffer (don't free))
                  clone_heap.free(send);
                  close(sk);
                  return reinterpret_cast<Packet*>(rec_buffer);
               }
            }
            fprintf(stderr, "> Blocking comm timed out (%s)\n", id);
            clone_heap.free(send);
            clone_heap.free(rec_buffer);
            close(sk);
            return NULL;

         }


         static void acquire_pt_lock() {
            /**
             *
             */
            uint8_t* send_buffer      = NULL;
            uint8_t* rec_buffer       = NULL;
            uint32_t sk               =  0;
            uint32_t nbytes           =  0;
            uint32_t addrlen          =  0;
            uint32_t selflen          =  0;
            uint32_t timeout          =  0;
            struct   sockaddr_in addr = {0};
            struct   sockaddr_in self = {0};

            Packet*            p      = NULL;
            Packet*            rec    = NULL;
            AcquireWriteLock*  acq    = NULL;
            ReleaseWriteLock*  rel    = NULL;
            GenericPacket*     gen    = NULL;
            SyncPage*          syncp  = NULL;

            selflen = sizeof(self);

            // If write lock is not in init state
            if (pt_owner != -1
                  // And we do not own write lock on page table
                  && local_addr.s_addr != pt_owner) {

               // Start a new listener
               sk = new_comm();

               // We care about the name of this socket
               getsockname(sk, (struct sockaddr*) &self, &selflen);

               // Lock the page table
               //mutex_lock(&pt_lock);
               // Owner will sync before releasing lock, so erase local pt
               //zero_pt();

               // Build acquire lock packet
               send_buffer = (uint8_t*) clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
               acq         = new (send_buffer) AcquireWriteLock(self.sin_port);
               timeout     = 5; // seconds

               fprintf(stderr, "> Asking %s for lock (sync to %d).\n",
                     inet_ntoa((struct in_addr&) pt_owner),
                     ntohs(acq->ret_port));

               addr.sin_family        = AF_INET;
               addr.sin_addr.s_addr   = pt_owner;
               addr.sin_port          = htons(self.sin_port);

               // Send packet, wait for response
               rec = persistent_blocking_comm(sk, (struct sockaddr*) &addr, acq, timeout, "acquire pt lock (1)");

               while (rec->get_flag() != RELEASE_WRITE_LOCK_F) {

                  if (rec->get_flag() == SYNC_PAGE_F) {
                     syncp = reinterpret_cast<SyncPage*>(rec);
                     fprintf(stderr, "> directly received sync page (%p)\n", (void*) ntohl(syncp->page_offset));
                     // Sync the page (assume page table is already locked)
                     memcpy((void*) ntohl(syncp->page_offset),
                           syncp->get_payload_ptr(),
                           PAGE_SZ);
                     // Ack receipt
                     gen = new (send_buffer) GenericPacket(SYNC_PAGE_ACK_F);
                  }
                  else if (rec->get_flag() == SYNC_DONE_F) {
                     // Ack receipt
                     gen = new (send_buffer) GenericPacket(SYNC_DONE_ACK_F);
                  }

                  // Send packet, wait for response
                  rec = persistent_blocking_comm(sk, (struct sockaddr*) &addr, acq, timeout, "acquire pt lock (2)");
               }

               if (rec->get_flag() == RELEASE_WRITE_LOCK_F) {
                  rel = reinterpret_cast<ReleaseWriteLock*>(rec);

                  fprintf(stderr, "> Received write lock.  Next avail memory = %p\n",
                        (void*) ntohl(rel->next_addr));

                  next_addr = ntohl(rel->next_addr);
                  // Take ownership of write lock
                  pt_owner = local_addr.s_addr;
               }

               // TODO: need to re-route packets to new owner sometimes

               // Release memory
               clone_heap.free(rec);
               clone_heap.free(send_buffer);
               // Close socket
               close(sk);
            }
            else {
               //fprintf(stderr, "> Already own lock\n");
            }

            return;
         }


         static uint32_t net_pthread_join(pthread_t thread_id) {
            /**
             * TODO: implement me
             */
            void*               packet_memory  = NULL;
            void*               work_memory    = NULL;
            uint32_t            timeout        = 0;
            struct sockaddr_in* owner          = NULL;
            Packet*             p              = NULL;

            owner = reinterpret_cast<struct sockaddr_in*>(&thread_map[thread_id]);
            packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

            fprintf(stderr, "> pthead join: contact %s:%d for thread\n",
                  inet_ntoa((struct in_addr&) owner->sin_addr),
                  ntohs(owner->sin_port));

            timeout = 1000000;

            p = ClusterCoordinator::blocking_comm(
                  (struct sockaddr*) owner,
                  reinterpret_cast<Packet*>(
                     new (packet_memory) ThreadJoin()),
                  timeout,
                  "thread join"
                  );

            if (p->get_flag() != THREAD_JOIN_ACK_F)
               fprintf(stderr, "> Bad thread join ack (%x)!\n", p->get_flag());

            clone_heap.free(p);

            return 0;
         }


         static uint32_t net_pthread_create(threadFunctionType start_routine, void* arg) {
            /**
             *
             */
            void*               packet_memory  = NULL;
            void*               work_memory    = NULL;
            void*               raw            = NULL;
            void*               thr_stack      = NULL;
            uint32_t            thr_stack_sz   = 0;
            uint32_t            remote_ip      = 0;
            uint32_t            timeout        = 0;
            uint32_t            thr_id         = 0;
            struct sockaddr_in  remote_addr    = {0};

            Packet*          p   = NULL;
            ThreadCreate*    tc  = NULL;
            ThreadCreateAck* tca = NULL;

            fprintf(stderr, "%lld >> pthread_create(%s) func(%p) arg(%p)\n", get_micro_clock(), local_ip, start_routine, arg);

            remote_ip = get_available_worker();

            fprintf(stderr, ">>>> send work to %s\n", inet_ntoa((struct in_addr&) remote_ip));

            remote_addr.sin_family      = AF_INET;
            remote_addr.sin_addr.s_addr = remote_ip;
            remote_addr.sin_port        = htons(UNICAST_PORT);

            work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
            packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

            timeout = 5;

            // Map this memory into our address space
            // TODO: make this position agnostic (could fail when mapped into other address space)
            // TODO: insert this memory into the page table
            //if((thr_stack = mmap(NULL,
            //            PTHREAD_STACK_SZ,
            //            PROT_NONE,
            //            MAP_SHARED | MAP_ANONYMOUS,
            //            -1, 0)) == MAP_FAILED) {
            //   fprintf(stderr, "> pthread stack map failed\n");
            //}
            // TODO: this is a hack.
            if ((thr_stack = malloc(PTHREAD_STACK_SZ + PAGE_SZ)) == NULL) {
               fprintf(stderr, "> pthread stack malloc failed\n");
            }

            // Sync page table with available worker
            mutex_lock(&pt_lock);
            active_pt_sync(remote_addr);
            mutex_unlock(&pt_lock);

            // Notify available worker to start thread
            p = ClusterCoordinator::blocking_comm(
                  (struct sockaddr*) &remote_addr,
                  reinterpret_cast<Packet*>(
                     new (packet_memory) ThreadCreate((void*) thr_stack, (void*) start_routine, (void*) arg)),
                  timeout,
                  "thread create"
                  );

            if (p->get_flag() == THREAD_CREATE_ACK_F) {
               tca = reinterpret_cast<ThreadCreateAck*>(p);

               thr_id = ntohl(tca->thread_id);

               fprintf(stderr, "> remote pthread id: %d\n", thr_id);

               // Set node state to ACTIVE
               mutex_lock(&pt_lock);
               node_list->find(remote_addr.sin_addr.s_addr)->second->status = MACHINE_ACTIVE;
               mutex_unlock(&pt_lock);

               fprintf(stderr, "> Send pthread_join (%d) to %s:%d\n",
                     thr_id,
                     inet_ntoa(remote_addr.sin_addr),
                     ntohs(remote_addr.sin_port));

               // Save (retaddr -> thr_id) for joining later
               thread_map.insert(
                     std::pair<uint32_t, struct sockaddr>
                     (thr_id, *((struct sockaddr*) &remote_addr)));

            }
            else {
               fprintf(stderr, "> Weird response (%x)\n", p->get_flag());
            }


            // This was returned to us, we're done with it
            clone_heap.free(p);

            return thr_id;
         }


         static uint32_t select_call(int socket, int seconds, int useconds) {
            /**
             *
             */
            fd_set rfds;
            struct timeval tv;
            FD_ZERO(&rfds);
            FD_SET(socket, &rfds);
            tv.tv_sec = seconds;
            tv.tv_usec = useconds;
            return select(socket + 1, &rfds, NULL, NULL, &tv);
         }

      private:
         uint32_t multi_speaker_thread_id;
         uint32_t multi_listener_thread_id;
         uint32_t uni_speaker_thread_id;
         uint32_t uni_listener_thread_id;

         uint32_t worker0_thread_id;
   };
}

#endif
