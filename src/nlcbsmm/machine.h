#ifndef __MACHINE_H__
#define __MACHINE_H__

#include "constants.h"

namespace NLCBSMM {

   class Machine {
      public:
         uint32_t ip_address;
         uint32_t status;

         Machine(uint32_t _ip_address) {
            ip_address = _ip_address;
            status     = MACHINE_INIT;
         }

   }__attribute__((packed));

}

#endif
