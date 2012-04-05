#include <cstdio>

#include "constants.h"

/**
 * Offsets
 */

extern uint8_t* main;
extern uint8_t* _end;

intptr_t& global_main() {
   static intptr_t m = ((intptr_t) &main);
   return m;
}

intptr_t& global_end() {
   static intptr_t e = ((intptr_t) &_end);
   return e;
}

intptr_t& global_base() {
   // Page align the _end of the program + a page
   static intptr_t base = (((intptr_t) &_end) & ~(PAGE_SZ - 1)) + PAGE_SZ;
   return base;
}

intptr_t& get_workers_fixed_addr() {
   static intptr_t off = global_base();
   return off;
}

intptr_t& get_clonebase_fixed_addr() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) get_workers_fixed_addr()
       + (WORKER_STACK_SZ * 4));
   return off;
}

intptr_t& global_clone_alloc_heap() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) get_clonebase_fixed_addr()
       + (CLONE_STACK_SZ * 4));
   return off;
}

intptr_t& global_clone_heap() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) global_clone_alloc_heap()
       + CLONE_ALLOC_HEAP_SZ);
   return off;
}

intptr_t& global_page_table_mach_list() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) global_clone_heap()
       + CLONE_HEAP_SZ);
   return off;
}

intptr_t& global_page_table_obj() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) global_page_table_mach_list()
       + PAGE_TABLE_MACH_LIST_SZ);
   return off;
}

intptr_t& global_page_table() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) global_page_table_obj()
       + PAGE_TABLE_OBJ_SZ);
   return off;
}

intptr_t& global_page_table_alloc_heap() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) global_page_table()
       + PAGE_TABLE_SZ);
   return off;
}

intptr_t& global_page_table_heap() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) global_page_table_alloc_heap()
       + PAGE_TABLE_ALLOC_HEAP_SZ);
   return off;
}

intptr_t& global_application_heap() {
   static intptr_t off = (intptr_t)
      ((uint8_t*) global_page_table_heap()
       + PAGE_TABLE_HEAP_SZ
       // TODO: remove this (it's for debugging)
       + (PAGE_SZ + 0xA0000000));
   return off;
}

intptr_t& global_pt_start_addr() {
   /**
    * Return the first address where the "page table region" starts.
    *
    */
   return global_page_table_mach_list();
}
