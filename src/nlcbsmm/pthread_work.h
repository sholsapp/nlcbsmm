#ifndef __PTHREAD_WORK_H__
#define __PTHREAD_WORK_H__

namespace NLCBSMM {

   class PthreadWork {
      public:
         uint32_t func;
         uint32_t arg;

         PthreadWork(uint32_t _func, uint32_t _arg) :
            func(_func),
            arg(_arg) {}

   }__attribute__((packed));

}

#endif
