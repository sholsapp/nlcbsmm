#include "location.h"

/**
 * Bitwise flags for a Page's state
 */
#define INIT          0x00000000
#define COPY_ON_WRITE 0x00000001

namespace NLCBSMM {


   class Page {
      public:
         Page() {
            loc = NULL;
            state = INIT;
         }

         Location* getLocation() {return loc;}
         void setLocation(Location* l) {loc = l;}

         unsigned int getState() {return state;}
         void setState(int s) {state = s;}

         bool isCOW() {return state & COPY_ON_WRITE;}

      private:
         Location* loc;
         unsigned int state;

   }__attribute__((packed));
}
