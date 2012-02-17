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
         uint32_t address;
         uint32_t protection;
         uint32_t version;

         Page(uint32_t _address, uint32_t _protection) {
            address    = _address;
            protection = _protection;
            version    = 0;
         }

   }__attribute__((packed));
}

#endif
