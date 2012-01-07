/**
 * If a class needs to allocate memory, it will have to use this custom allocator.
 */ 
#include "freelistheap.h" 
#include "mmapheap.h" 

using namespace HL;

namespace NLCBSMM {

  extern FreelistHeap<MmapHeap> myheap;

}
