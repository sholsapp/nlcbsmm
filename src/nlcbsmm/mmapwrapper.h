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
         enum { Size      = 4 * 1024 };
         enum { Alignment = 4 * 1024 };

         static void * map (size_t sz) {
            /**
             *
             */
            void*    ptr              = NULL;
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

            if (sz == 0) {
               return NULL;
            }

            mutex_lock(&pt_owner_lock);

            //fprintf(stderr, "> pt_owner = %s\n", inet_ntoa((struct in_addr&) pt_owner));

            // If write lock is not in init state
            if (pt_owner != -1
                  // And we do not own write lock on page table
                  && local_addr.s_addr != pt_owner) {

               fprintf(stderr, "> Asking %s for lock.\n",
                     inet_ntoa((struct in_addr&) pt_owner));

               send_buffer = (uint8_t*) clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

               rec = ClusterCoordinator::blocking_comm(
                     pt_owner, 
                     new (send_buffer) AcquireWriteLock(), 
                     5);

               if (rec->get_flag() == RELEASE_WRITE_LOCK_F) {
                  rel = reinterpret_cast<ReleaseWriteLock*>(rec);

                  fprintf(stderr, "> Received write lock (next = %p\n",
                        (void*) ntohl(rel->next_addr));

                  next_addr = ntohl(rel->next_addr);
                  // Take ownership of write lock
                  pt_owner = local_addr.s_addr;
               }
               else {
                  fprintf(stderr, "> Unknown packet response\n");
               }

               // TODO: need to re-route packets to new owner sometimes

               // Release memory
               clone_heap.free(send_buffer);
               clone_heap.free(rec);

               // Close socket
               close(sk);
            }
            else {
               fprintf(stderr, "> Already own lock\n");
            }

            // Allocate memory
            if ((ptr = mmap ((void*)next_addr,
                        sz,
                        HL_MMAP_PROTECTION_MASK,
                        MAP_ANONYMOUS | MAP_FIXED | MAP_PRIVATE,
                        -1, 0)) == MAP_FAILED) {
               fprintf (stderr, "Virtual memory exhausted.\n");
               mutex_unlock(&pt_owner_lock);
               return NULL;
            }
            else {
               // Alert NLCBSMM of new memory
               init_nlcbsmm_memory(ptr, sz);
               // Increment the next address the allocator may pull from
               next_addr = (uint32_t) (((uint8_t*) next_addr) + sz);
               mutex_unlock(&pt_owner_lock);
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
            uint32_t mach_status = 0;
            uint8_t* block_addr  = NULL;
            uint8_t* page_addr   = NULL;
            void* work_memory    = NULL;
            void* packet_memory  = NULL;
            void* raw            = NULL;

            struct sockaddr_in fake = {0};

            if ((sz % Size) == 0) {
               // The number of pages being allocated
               page_count = sz / Size;
            }
            else {
               fprintf(stderr, "> FAIL: Hoard asked for non-page memory amount?!\n");
               return;
            }

            if (node_list->count(local_addr.s_addr) == 0) {
               fprintf(stderr, "> Adding %s to node list\n", inet_ntoa(local_addr));
               raw = get_pt_heap(&pt_lock)->malloc(sizeof(Machine));
               node_list->insert(
                     std::pair<uint32_t, Machine*>(
                        local_addr.s_addr,
                        new (raw) Machine(local_addr.s_addr))
                     );
            }

            // TODO: error checking
            mach_status = node_list->find(local_addr.s_addr)->second->status;


            fprintf(stderr, "> %s state %d\n", inet_ntoa(local_addr), mach_status);

            if (mach_status == MACHINE_ACTIVE
                  || mach_status == MACHINE_MASTER) {

               work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
               packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);

               // Push work onto broadcast speaker's queue
               safe_push(&multi_speaker_work_deque, &multi_speaker_lock,
                     // A new work tuple
                     new (work_memory) WorkTupleType(fake,
                        // A new packet
                        new (packet_memory) SyncReserve(local_addr.s_addr, ptr, sz))
                     );
            }

            // This should already be page algined, but w/e
            block_addr = pageAlign((uint8_t*) ptr);
            page_addr = NULL;

            for (int page = 0; page < page_count; page++) {
               page_addr = block_addr + (page * PAGE_SZ);
               //fprintf(stderr, "Superblock (%p) - Page (%p)\n", block_addr, page_addr);
               raw = get_pt_heap(&pt_lock)->malloc(sizeof(Page));
               page_table->insert(
                     std::pair<uint32_t, PageTableElementType>((uint32_t) page_addr,
                        PageTableElementType(
                           // A new page
                           new (raw) Page((uint32_t) page_addr, 0xD010101D),
                           // Pointer to this machine (in the node list)
                           node_list->find(local_addr.s_addr)->second
                           ))
                     );
            }

            return;
         }
   };
}

#endif
