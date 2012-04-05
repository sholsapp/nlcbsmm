#ifndef __PTHREAD_WORK_H__
#define __PTHREAD_WORK_H__

namespace NLCBSMM {

   class PthreadWork {
      public:
         intptr_t func;
         intptr_t arg;
         intptr_t stack_ptr;

         PthreadWork(intptr_t _func, intptr_t _arg, intptr_t _stack_ptr) :
            func(_func),
            arg(_arg),
            stack_ptr(_stack_ptr) {}

   }__attribute__((packed));

}

#endif
