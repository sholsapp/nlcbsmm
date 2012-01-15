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

}
