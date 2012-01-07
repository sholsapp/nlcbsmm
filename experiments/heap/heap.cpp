#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <sys/mman.h>
#include <string>

using namespace std;

struct SBEntry {
   unsigned char* addr;
}__attribute__((packed)); 


struct TreeNode {
   struct SBEntry* data;
   TreeNode* left;
   TreeNode* right;
}__attribute__((packed));

/*
 * This object sets up the internal memory space and acts as an
 *  allocator for memory in that space.
 */
template <typename META, typename DATA>
class InternalMemoryManager {
   public:
      /*
       * Constructor is emptry so we can init on the global stack.
       */
      InternalMemoryManager() {
         offset_meta = -1;
         offset_data = -1;
         initialized = false;
      }

      /*
       * Must be called before buliding the data structure so we have memory.
       */
      void init(unsigned char* ptr, unsigned int size) {
         // meta data is stored at 0
         offset_meta = 0;

         // actual data is stored starting in the middle of internal memory
         offset_data = size / 2;

         // duh
         memory = ptr;

         // for the assert statements
         initialized = true;
         return;
      }

      META* allocNode() {
         assert(initialized);
         // interpret the address as a new metadata node
         META* retAddr = reinterpret_cast<META*>(memory + offset_meta);
         //record how much memory was saved
         offset_meta += sizeof(META);
         return retAddr;
      }

      DATA* allocData() {
         assert(initialized);
         // interpret the address as a new data node
         DATA* retAddr = reinterpret_cast<DATA*>(memory + offset_data);
         //record how much memory was saved
         offset_data += sizeof(DATA);
         return retAddr;
      }

   private:
      unsigned int offset_meta;
      unsigned int offset_data;
      unsigned char* memory;
      bool initialized;
};

template <typename T>
class LinkedList {
   public:
      /*
       * Constructor
       */
      LinkedList(InternalMemoryManager<struct TreeNode, struct SBEntry>* memorySource) {
        head = tail = NULL;
        source = memorySource;
        sz = 0;
      }

      /*
       * Insert an element into the data structure
       */
      void insert(T* data) {
         if (head == NULL) {
           fprintf(stderr, "head == NULL\n");
           head = source->allocNode();
           fprintf(stderr, "allocNode returned %p\n", head);
           head->data = data;
           head->right = NULL;
           head->left = NULL;
           tail = head;
         }
         else {

           tail->right = source->allocNode();
           tail->right->data = data;
           tail->right->right = NULL;
           tail->right->left = NULL;
           tail = tail->right;
         }
         sz++;
      }

      /*
       * Look up an element in the data structure
       */
      T* lookup(T* item) {
        TreeNode* temp = head;
        while (temp != NULL) {
          if (temp->data == item->addr) {
            return temp->data;
          }
          else {
            temp = temp->right;
          }
        }
      }

      /*
       * Get the size of the list
       */
      unsigned int size() {
         return sz;
      }

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
      InternalMemoryManager<struct TreeNode, struct SBEntry>* source;

};





// Global data
InternalMemoryManager<struct TreeNode, struct SBEntry> mm;
LinkedList<struct SBEntry> metadata(&mm);

int main(int argc, char** argv) {

   unsigned int size = 1024 * 2;
   unsigned char* memory = (unsigned char*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

   fprintf(stderr, "MEMORY RANGE: %p -> %p\n", memory, memory + size);

   mm.init(memory, size);

   struct SBEntry* entry1 = (struct SBEntry*)mm.allocData();
   struct SBEntry* entry2 = (struct SBEntry*)mm.allocData();

   entry1->addr = (unsigned char*) 19;
   fprintf(stderr, "entry1->addr = %p\n", entry1->addr);
   entry2->addr = (unsigned char*) 20;
   fprintf(stderr, "entry2->addr = %p\n", entry2->addr);

   metadata.insert(entry1);
   metadata.insert(entry2);

   return 0;
}




