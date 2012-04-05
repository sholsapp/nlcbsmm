#ifndef __MACHINE_H__
#define __MACHINE_H__

#include "constants.h"

namespace NLCBSMM {

   class Machine {
      public:
         int ip_address;
         int status;

         Machine(int _ip_address) {
            ip_address = _ip_address;
            status     = MACHINE_INIT;
         }

   }__attribute__((packed));

}

#endif
