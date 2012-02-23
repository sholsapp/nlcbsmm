#ifndef __PTHREAD_WORK_H__
#define __PTHREAD_WORK_H__

namespace NLCBSMM {

   class PthreadWork {
      public:
         uint32_t func;
         uint32_t arg;
         uint32_t stack_ptr;

         PthreadWork(uint32_t _func, uint32_t _arg, uint32_t _stack_ptr) :
            func(_func),
            arg(_arg),
            stack_ptr(_stack_ptr) {}

   }__attribute__((packed));

}

#endif
