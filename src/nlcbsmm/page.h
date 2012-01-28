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
         //struct sockaddr_in owner;

         Page(uint32_t _address) {
            address = _address;
            //owner.sin_family = AF_INET;
            //owner.sin_addr.s_addr = htonl(INADDR_ANY);
            //owner.sin_port = htons(UNICAST_PORT);
         }

   }__attribute__((packed));
}
