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
            void* ptr = NULL;

            if (sz == 0) {
               return NULL;
            }

            mutex_lock(&pt_owner_lock);

            // Acquire page table owner lock (so we can allocate)
            ClusterCoordinator::acquire_pt_lock();

            // Allocate memory
            if ((ptr = mmap ((void*) next_addr,
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

            
            mutex_lock(&node_list_lock);

            if (node_list->count(local_addr.s_addr) == 0) {
               fprintf(stderr, "> Adding %s to node list\n", inet_ntoa(local_addr));
               raw = pt_heap->malloc(sizeof(Machine));//BAD:)
               node_list->insert(
                     std::pair<uint32_t, Machine*>(
                        local_addr.s_addr,
                        new (raw) Machine(local_addr.s_addr))
                     );
            }

            // TODO: error checking
            mach_status = node_list->find(local_addr.s_addr)->second->status;

            //fprintf(stderr, "> %s state %d\n", inet_ntoa(local_addr), mach_status);
            //if (mach_status == MACHINE_ACTIVE
            //      || mach_status == MACHINE_MASTER) {
            //   work_memory   = clone_heap.malloc(sizeof(WorkTupleType));
            //   packet_memory = clone_heap.malloc(sizeof(uint8_t) * MAX_PACKET_SZ);
            //   safe_push(&multi_speaker_work_deque, &multi_speaker_lock,
            //         new (work_memory) WorkTupleType(fake,
            //            new (packet_memory) SyncReserve(local_addr.s_addr, ptr, sz))
            //         );
            //}
            usleep(500000);

            // This should already be page algined, but w/e
            block_addr = pageAlign((uint8_t*) ptr);
            page_addr = NULL;

            for (int page = 0; page < page_count; page++) {
               page_addr = block_addr + (page * PAGE_SZ);
               //fprintf(stderr, "Superblock (%p) - Page (%p)\n", block_addr, page_addr);
               raw = pt_heap->malloc(sizeof(Page));
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

            mutex_unlock(&node_list_lock);

            return;
         }
   };
}

#endif
