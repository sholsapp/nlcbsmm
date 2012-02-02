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

         Page(uint32_t _address, uint32_t _protection) {
            address    = _address;
            protection = _protection;
         }

   }__attribute__((packed));
}
