/* -*- C++ -*- */

/*

   Heap Layers: An Extensible Memory Allocation Infrastructure

   Copyright (C) 2000-2004 by Emery Berger
http://www.cs.umass.edu/~emery
emery@cs.umass.edu

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */

#ifndef _NLCBSMMMMAPHEAP_H_
#define _NLCBSMMMMAPHEAP_H_

// UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <map>

#include "sassert.h"
#include "myhashmap.h"
#include "stlallocator.h"
#include "freelistheap.h"
#include "zoneheap.h"
#include "lockedheap.h"
#include "spinlock.h"
#include "hldefines.h"

// NLCBSMM
#include "constants.h"

#if HL_EXECUTABLE_HEAP
#define HL_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)
#else
#define HL_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE)
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif


#include <new>

/**
 * @class MmapHeap
 * @brief A "source heap" that manages memory via calls to the VM interface.
 * @author Emery Berger
 */

namespace HL {

   template<int heap_identifier>
      class NlcbsmmChunkHeap {
         /**
          * The NlcbsmmChunkHeap is a chunk heap that grabs memory from a specific
          * region of memory, depending on the template heap_identifier.
          */
         public:

            enum { Alignment = 4 * 1024 };

            NlcbsmmChunkHeap() :
               /**
                *
                */
               buffer(NULL),
               count(0) {}


            inline void * malloc (size_t sz) {
               /**
                *
                */
               void* ptr = NULL;

               //IF the buffer is NULL (First malloc call)
               if(buffer == NULL) {
                  //SETUP the buffer based on which heap this allocator is for
                  switch (heap_identifier) {

                  case CLONE_ALLOC_HEAP_START:
                     buffer = mmap((void*) CLONE_ALLOC_HEAP_OFFSET, CLONE_ALLOC_HEAP_SZ,
                           PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                     break;

                  case CLONE_HEAP_START:
                     buffer = mmap((void*) CLONE_HEAP_OFFSET, CLONE_HEAP_SZ,
                           PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

                     break;

                  case PAGE_TABLE_ALLOC_HEAP_START:
                     buffer = mmap((void*) PAGE_TABLE_ALLOC_HEAP_OFFSET, PAGE_TABLE_ALLOC_HEAP_SZ,
                           PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

                     break;

                  case PAGE_TABLE_HEAP_START:
                     buffer = mmap((void*) PAGE_TABLE_HEAP_OFFSET, PAGE_TABLE_HEAP_SZ,
                           PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

                     break;

                  default:
                     fprintf(stderr, "UNKNOWN heap_indentifier %d\n", heap_identifier);
                     break;
                  }
                  count = 0;
               }

               //IF the mmap failed return null
               if (buffer == MAP_FAILED) {
                  fprintf(stderr, "> Mapping failed\n");
                  ptr = NULL;
               }
               else {
                  // Return a ptr to a new piece of memory
                  //fprintf(stderr, "> allocating %d\n", sz);
                  ptr = buffer;
                  buffer = reinterpret_cast<void *>(reinterpret_cast<uint32_t>(buffer) + sz);
                  count++;
               }
               return ptr;
            }


            inline void free (void * ptr) {
               /**
                *
                */
               munmap (reinterpret_cast<char *>(ptr), getSize(ptr));
            }


            inline size_t getSize (void * ptr) {
               /**
                *
                */
               ptr = ptr;
               //abort();
               return Alignment; // Obviously broken. Such is life.
            }


            void free (void * ptr, size_t sz) {
               /**
                *
                */
               if ((long) sz < 0) {
                  abort();
               }

               munmap (reinterpret_cast<char *>(ptr), sz);
            }
         private:

            void* buffer;

            unsigned int count;
      };


   template<int heap_identifier>
      class NlcbsmmMmapHeap : public NlcbsmmChunkHeap<heap_identifier> {
         /**
          * This is the public interface of the NlcbsmmMmapHeap and is thread-safe.
          */

         private:

            /**
             * FIX ME: 16 = size of ZoneHeap header.
             */
            class MyHeap :
               public LockedHeap<SpinLockType, FreelistHeap<ZoneHeap<NlcbsmmChunkHeap<heap_identifier + 1>, 16384 - 16> > > {};

            typedef MyHashMap<void *, size_t, MyHeap> mapType;

         protected:
            mapType MyMap;

            SpinLockType MyMapLock;

         public:

            inline void * malloc (size_t sz) {
               /**
                *
                */
               void * ptr = NlcbsmmChunkHeap<heap_identifier>::malloc (sz);
               MyMapLock.lock();
               MyMap.set (ptr, sz);
               MyMapLock.unlock();
               assert (reinterpret_cast<size_t>(ptr) % Alignment == 0);
               return const_cast<void *>(ptr);
            }


            inline size_t getSize (void * ptr) {
               /**
                *
                */
               MyMapLock.lock();
               size_t sz = MyMap.get (ptr);
               MyMapLock.unlock();
               return sz;
            }


            void free (void * ptr, size_t sz) {
               /**
                * WORKAROUND: apparent gcc bug.
                */
               NlcbsmmChunkHeap<heap_identifier>::free (ptr, sz);
            }


            inline void free (void * ptr) {
               /**
                *
                */
               assert (reinterpret_cast<size_t>(ptr) % Alignment == 0);
               MyMapLock.lock();
               size_t sz = MyMap.get (ptr);
               NlcbsmmChunkHeap<heap_identifier>::free (ptr, sz);
               MyMap.erase (ptr);
               MyMapLock.unlock();
            }
      };
}
#endif
