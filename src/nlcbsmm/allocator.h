/**
 * If a class needs to allocate memory, it will have to use this custom allocator.
 */
#include <assert.h>
#include "firstfitheap.h"
#include "nlcbsmmheap.h"

using namespace HL;

namespace NLCBSMM {
   extern FirstFitHeap<NlcbsmmMmapHeap<CLONE_HEAP_START> >             clone_heap;
   extern FirstFitHeap<NlcbsmmMmapHeap<PAGE_TABLE_HEAP_START> >*        pt_heap;
}
