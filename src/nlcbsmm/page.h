#ifndef __PAGE_H__
#define __PAGE_H__

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "constants.h"

namespace NLCBSMM {

   class Page {
      public:
         intptr_t address;
         int protection;
         int version;
         int dirty;

         Page(intptr_t _address, int _protection) {
            address    = _address;
            protection = _protection;
            version    = 0;
            dirty      = 0;
         }

   }__attribute__((packed));
}

#endif
