#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <sys/mman.h>
#include <cstring>

#define PAGESIZE 4096

#include "allocator.h"
#include "page.h"

using namespace HL;

namespace NLCBSMM {

   struct SBEntry {
      //  A pointer to a superblock type
      void* sb;
      // A pointer to a Page
      Page page[16];
   }__attribute__((packed));

   struct TreeNode {
      struct SBEntry* data;
      TreeNode* left;
      TreeNode* right;
   }__attribute__((packed));

   /**
    * This class is a linked list where we store metadata about each allocated superblock
    */
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

         int getSize();

         SBEntry* get(int index);
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
         FreelistHeap<MmapHeap>* heap;

   };
}
