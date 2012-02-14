#ifndef _MMAPWRAPPER_H_
#define _MMAPWRAPPER_H_

// UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <map>

#if HL_EXECUTABLE_HEAP
#define HL_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)
#else
#define HL_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE)
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace HL {

   class MmapWrapper {
      public:

         // Linux and most other operating systems align memory to a 4K boundary.
         enum { Size = 4 * 1024 };
         enum { Alignment = 4 * 1024 };

         static void * map (size_t sz) {
            /**
             *
             */
            void*    ptr;
            uint8_t* send_buffer      = NULL;
            uint8_t* rec_buffer       = NULL;
            uint32_t sk               =  0;
            uint32_t nbytes           =  0;
            uint32_t addrlen          =  0;
            uint32_t selflen          =  0;
            uint32_t yes              =  1;
            struct   ip_mreq mreq     = {0};
            struct   sockaddr_in addr = {0};
            struct   sockaddr_in self = {0};

            Packet*            p      = NULL;
            AcquireWriteLock*  acq    = NULL;
            ReleaseWriteLock*  rel    = NULL;

            if (sz == 0) {
               return NULL;
            }

            mutex_lock(&pt_owner_lock);

            fprintf(stderr, "> pt_owner = %s\n", inet_ntoa((struct in_addr&) pt_owner));

            // If write lock is not in init state
            if (pt_owner != -1
                  // AND we do not own write lock on page table
                  && local_addr.s_addr != pt_owner) {
               fprintf(stderr, "> Not owner, asking %s for lock.\n", inet_ntoa((struct in_addr&) pt_owner));

               // Setup client/server to block until lock is acquired
               if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                  perror("vmmanager.cpp, socket");
                  exit(EXIT_FAILURE);
               }

               self.sin_family      = AF_INET;
               self.sin_port        = 0;
               selflen              = sizeof(self);
             
               addr.sin_family      = AF_INET;
               addr.sin_addr.s_addr = pt_owner;
               addr.sin_port        = htons(UNICAST_PORT);
               addrlen              = sizeof(addr);

               if (bind(sk, (struct sockaddr *) &self, selflen) < 0) {
                  perror("vmmanager.cpp, bind");
                  exit(EXIT_FAILURE);
               }

               send_buffer = (uint8_t*) clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

               acq = new (send_buffer) AcquireWriteLock();

               // Send request to acquire write lock
               if (sendto(sk, send_buffer, MAX_PACKET_SZ, 0, (struct sockaddr *) &addr , sizeof(addr)) < 0) {
                  perror("vmmanager.cpp, sendto");
                  exit(EXIT_FAILURE);
               }

               rec_buffer = (uint8_t*) clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

               // TODO: select for lock, handle errors or timeout

               // Wait for owner to release write lock
               if ((nbytes = recvfrom(sk, rec_buffer, MAX_PACKET_SZ, 0, (struct sockaddr *) &addr, &addrlen)) < 0) {
                  perror("recvfrom");
                  exit(EXIT_FAILURE);
               }

               p = reinterpret_cast<Packet*>(rec_buffer);

               if (p->get_flag() == RELEASE_WRITE_LOCK_F) {
                  rel = reinterpret_cast<ReleaseWriteLock*>(rec_buffer);
                  fprintf(stderr, "> Received write lock\n");
                  // Take ownership of write lock
                  pt_owner = ntohl(local_addr.s_addr);
               }
               else {
                  fprintf(stderr, "> Unknown packet response\n");
               }

               // TODO: need to re-route packets to new owner sometiems

               // Close socket
               close(sk);
            }
            else {
               fprintf(stderr, "> Already own lock\n");
            }

            mutex_unlock(&pt_owner_lock);

            // Allocate memory
            ptr = mmap (0, sz, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

            if (ptr == MAP_FAILED) {
               fprintf (stderr, "Virtual memory exhausted.\n");
               return NULL;
            } else {
               // Alert NLCBSMM of new memory
               init_nlcbsmm_memory(ptr, sz);
               return ptr;
            }

         }

         static void unmap (void * ptr, size_t sz) {
            munmap (reinterpret_cast<char *>(ptr), sz);
         }

         static void init_nlcbsmm_memory (void* ptr, size_t sz) {
            /**
             * After Hoard asks the OS for another Superblock, we need to record
             * the address and protection information into the distributed page_table.
             */

            fprintf(stderr, "> NLCBSMM memory init %p(%d)\n", (void*) ptr, sz);

            uint32_t page_count  = 0;
            uint8_t* block_addr  = NULL;
            uint8_t* page_addr   = NULL;
            void* work_memory    = NULL;
            void* packet_memory  = NULL;

            struct sockaddr_in fake = {0};

            if ((sz % Size) == 0) {
               // The number of pages being allocated
               page_count = sz / Size;
            }
            else {
               fprintf(stderr, "> FAIL: Hoard asked for non-page memory amount?!\n");
               return;
            }

            work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
            packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

            // Push work onto broadcast speaker's queue
            safe_push(&multi_speaker_work_deque, &multi_speaker_lock,
                  // A new work tuple
                  new (work_memory) WorkTupleType(fake,
                     // A new packet
                     new (packet_memory) SyncReserve(inet_addr(local_ip), ptr, sz))
                  );

            // Does this node exist in the page table?
            if (page_table->count(inet_addr(local_ip)) == 0) {
               fprintf(stderr, "> Adding %s to page_table\n", local_ip);
               // Init a new vector for this node
               page_table->insert(
                     // IP -> std::vector<Page>
                     std::pair<uint32_t, PageVectorType*>(
                        inet_addr(local_ip),
                        new (get_pt_heap(&pt_lock)->malloc(sizeof(PageVectorType))) PageVectorType()));
            }

            // This should already be page algined, but w/e
            block_addr = pageAlign((uint8_t*) ptr);
            page_addr = NULL;

            for (int page = 0; page < page_count; page++) {
               page_addr = block_addr + (page * PAGE_SZ);
               //fprintf(stderr, "Superblock (%p) - Page (%p)\n", block_addr, page_addr);
               page_table->find(inet_addr(local_ip))->second->push_back(
                     new (get_pt_heap(&pt_lock)->malloc(sizeof(Page)))
                     Page((uint32_t) page_addr,
                        0xD010101D));
            }
         }
   };
}

#endif
