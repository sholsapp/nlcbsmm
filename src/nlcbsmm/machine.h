#ifndef __MACHINE_H__
#define __MACHINE_H__

#include "constants.h"

namespace NLCBSMM {

   class Machine {
      public:
         uint32_t ip_address;

         Machine(uint32_t _ip_address) {
            ip_address = _ip_address;
         }

   }__attribute__((packed));

}

#endif
