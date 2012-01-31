/**
 * If a class needs to allocate memory, it will have to use this custom allocator.
 */
#include "mmapheap.h"
#include "freelistheap.h"

#include "firstfitheap.h"
#include "nlcbsmmheap.h"

using namespace HL;

namespace NLCBSMM {

   extern FreelistHeap<MmapHeap> myheap;

   extern FirstFitHeap<NlcbsmmMmapHeap<CLONE_ALLOC_HEAP_START> >       clone_alloc_heap;
   extern FirstFitHeap<NlcbsmmMmapHeap<CLONE_HEAP_START> >             clone_heap;
   extern FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_ALLOC_HEAP_START> >  pt_alloc_heap;
   extern FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_HEAP_START> >        pt_heap;

}
