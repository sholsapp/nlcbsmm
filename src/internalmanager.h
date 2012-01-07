#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <sys/mman.h>
#include <cstring>
#include <zlib.h>

#include "freelistheap.h"
#include "mmapheap.h"
#include "mallocheap.h"

#define PAGESIZE 4096

using namespace HL;

namespace NLCBSMM {

   // Super class to represent a location of a page
   class Location {
      public:
         virtual int getSize() = 0;
         virtual void* getPage() = 0;
   };

   // Class to represent a page that resides in a chunk of memory
   class MemoryLocation : public Location {
      public:
         MemoryLocation(void* ptr) {
            actualPage = ptr;
            sz = 4096;
         }

         virtual int getSize() {
            return sizeof(MemoryLocation);
         }

         virtual void* getPage() {
            return actualPage;
         }

      private:
         void* actualPage;
         int sz;
   };

   class ShadowMemoryLocation : public Location {
      public:
         ShadowMemoryLocation() {
            actualPage = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            sz = 4096;
         }

         ~ShadowMemoryLocation() {
            munmap(actualPage, PAGESIZE);
         }

         virtual int getSize() {
            return PAGESIZE;
         }

         virtual void* getPage() {
            //return NULL;
            return actualPage;
         }

         /**
          * Takes a pointer to the "real" page and copies
          *  the contents of actualPage to it.
          */
         void setPage(void* page) {
            //move the page
            memcpy(actualPage, page, PAGESIZE);

            fprintf(stderr, "Zeroing page at %p\n", page);

            //make that page unreadable
            memset(page, 0, 4096);
            return;
         }

      private:
         void* actualPage;
         int sz;
   };


   // Class to represent a page that resides in a file
   class FileLocation : public Location {
      public:
         virtual int getSize() {
            return sizeof(FileLocation);
         }
         virtual void* getPage() {
            return NULL;
         }
   };

   struct SBEntry {
      /*
       * A pointer to a superblock type
       *  Usage: SuperblockType* t = reinterpret_cast<SuperblockType*> (addr);
       */
      void* sb;

      /*
       * A pointer to a location for each of the 16 pages in this superblock
       */
      Location* loc[16];

   }__attribute__((packed));

   struct TreeNode {
      struct SBEntry* data;
      TreeNode* left;
      TreeNode* right;
   }__attribute__((packed));

   // Some global statistics to screw around with ~ sholsapp
   extern int number_of_mallocs;
   extern int number_of_frees;

   /*
    * This object sets up the internal memory space and acts as an
    *  allocator for memory in that space.
    */
   class InternalMemoryManager {
      public:
         /*
          * Constructor is emptry so we can init on the global stack.
          */
         InternalMemoryManager() {
            offset_meta = -1;
            offset_data = -1;
            offset_obj = -1;
            initialized = false;
         }

         /*
          * Must be called before buliding the data structure so we have memory.
          */
         void init(void* ptr, void* obj_ptr, unsigned int size) {
            // meta data is stored at 0
            offset_meta = 0;

            // actual data is stored starting in the middle of internal memory
            offset_data = size / 2;

            // object data is stored at 0
            offset_obj = 0;

            // duh
            memory = (unsigned char*) ptr;

            // duh
            obj_memory = (unsigned char*) obj_ptr;

            fprintf(stderr, "INTERNAL MEMORY INIT (%p, %p)\n", ptr, obj_ptr);

            // for the assert statements
            initialized = true;
            return;
         }

         /*
          * Returns a pointer to the base of the memory
          */
         void* getBase() {
            return (void*) memory;
         }

         /*
          * Allocates a node for storing metadata
          */
         TreeNode* allocNode() {
            assert(initialized);
            // interpret the sbess as a new metadata node
            TreeNode* retAddr = reinterpret_cast<TreeNode*>(memory + offset_meta);
            //record how much memory was saved
            offset_meta += sizeof(TreeNode);
            return retAddr;
         }

         /*
          * Allocates a node for storing data
          */
         SBEntry* allocData() {
            assert(initialized);
            // interpret the sbess as a new data node
            SBEntry* retAddr = reinterpret_cast<SBEntry*>(memory + offset_data);
            //record how much memory was saved
            offset_data += sizeof(SBEntry);
            return retAddr;
         }

         /**
          * Allocates a piece of memory of a given size
          */
         void* allocObj(int sz) {
            assert(initialized);
            //fprintf(stderr, "allocObj(%d)\n", sz);
            void* retAddr = reinterpret_cast<void*>(obj_memory + offset_obj);
            // record how much memory was saved
            offset_obj += sz;
            return retAddr;
         }

      private:
         unsigned int offset_meta;
         unsigned int offset_data;
         unsigned int offset_obj;

         unsigned char* memory;
         unsigned char* obj_memory;

         bool initialized;
   };

   /**
    * This class is a linked list where we store metadata about each allocated superblock
    */
   //template <typename Entry>
   class LinkedList {
      public:
         /*
          * Constructor
          */
         LinkedList(FreelistHeap<MmapHeap>* memorySource) {
            head = tail = NULL;
            heap = memorySource;
            sz = 0;
         }


         SBEntry* findSuperblock(void* ptr);

         void insert(SBEntry* data);

      private:
         /*
          * The pointer to the base of the memory segment we may use.
          */
         TreeNode* head;

         /*
          * The pointer to the end of the list
          */
         TreeNode* tail;

         /*
          * The size of the list
          */
         unsigned int sz;

         /*
          * The memory allocator to use for the internal data structure (screw you malloc)
          */
         //InternalMemoryManager* source;

         FreelistHeap<MmapHeap>* heap;


   };
}
