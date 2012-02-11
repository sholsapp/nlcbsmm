#ifndef __PT_ALLOCATOR_H__
#define __PT_ALLOCATOR_H__

#include <limits>
#include <iostream>

#include "allocator.h"

namespace NLCBSMM {

   template <class T> class PageTableAllocator {
      /**
       * A custom allocator class for STL containers.
       *
       * This class makes use of a global variable to aquire and release memory.  Sorry... this is a prototype.
       *
       */
      public:
         // Type definitions
         typedef T              value_type;
         typedef T*             pointer;
         typedef const T*       const_pointer;
         typedef T&             reference;
         typedef const T&       const_reference;
         typedef std::size_t    size_type;
         typedef std::ptrdiff_t difference_type;


         /**
          * Constructors and destructor
          */
         PageTableAllocator() throw() {}
         PageTableAllocator(const PageTableAllocator&) throw() {}
         template <class U> PageTableAllocator (const PageTableAllocator<U>&) throw() {}
         ~PageTableAllocator() throw() {}


         // Rebind allocator to type U
         template <class U>
            struct rebind {
               typedef PageTableAllocator<U> other;
            };


         // Return address of values
         pointer address (reference value) const {
            return &value;
         }


         const_pointer address (const_reference value) const {
            return &value;
         }


         size_type max_size () const throw() {
            /**
             * Return maximum number of elements that can be allocated.
             */
            return std::numeric_limits<std::size_t>::max() / sizeof(T);
         }


         pointer allocate (size_type num, const void* = 0) {
            /**
             * Allocate but don't initialize num elements of type T.
             */
            std::cerr << "allocate " << num << " element(s)"
               << " of size " << sizeof(T) << std::endl;
            // The heap is coupled global variabled -- must be defined!
            pointer ret = (pointer)(pt_heap.malloc(num*sizeof(T)));
            std::cerr << " allocated at: " << (void*)ret << std::endl;
            return ret;
         }


         void construct (pointer p, const T& value) {
            /**
             * Initialize elements of allocated storage p with value value.
             */
            std::cerr << "construct element at " << p;
            new((void*)p)T(value);
         }


         void destroy (pointer p) {
            /**
             * Destroy elements of initialized storage p.
             */
            p->~T();
         }


         void deallocate (pointer p, size_type num) {
            /**
             * Deallocate storage p of deleted elements.
             */
            std::cerr << "deallocate " << num << " element(s)"
               << " of size " << sizeof(T)
               << " at: " << (void*)p << std::endl;
            // The heap is coupled global variabled -- must be defined!
            pt_heap.free((void*)p);
         }
   };


   // return that all specializations of this allocator are interchangeable
   template <class T1, class T2>
      bool operator== (const PageTableAllocator<T1>&,
            const PageTableAllocator<T2>&) throw() {
         return true;
      }


   template <class T1, class T2>
      bool operator!= (const PageTableAllocator<T1>&,
            const PageTableAllocator<T2>&) throw() {
         return false;
      }
}

#endif
