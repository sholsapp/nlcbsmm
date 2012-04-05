#include <cstdio>

#include "constants.h"

/**
 * Offsets
 */

extern uint8_t* main;
extern uint8_t* _end;

uint32_t& global_main() {
   static uint32_t m = ((uint32_t) &main);
   return m;
}

uint32_t& global_end() {
   static uint32_t e = ((uint32_t) &_end);
   return e;
}

uint32_t& global_base() {
   // Page align the _end of the program + a page
   static uint32_t base = (((uint32_t) &_end) & ~(PAGE_SZ - 1)) + PAGE_SZ;
   return base;
}

uint32_t& get_workers_fixed_addr() {
   static uint32_t off = global_base();
   return off;
}

uint32_t& get_clonebase_fixed_addr() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) get_workers_fixed_addr()
       + (WORKER_STACK_SZ * 4));
   return off;
}

uint32_t& global_clone_alloc_heap() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) get_clonebase_fixed_addr()
       + (CLONE_STACK_SZ * 4));
   return off;
}

uint32_t& global_clone_heap() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) global_clone_alloc_heap()
       + CLONE_ALLOC_HEAP_SZ);
   return off;
}

uint32_t& global_page_table_mach_list() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) global_clone_heap()
       + CLONE_HEAP_SZ);
   return off;
}

uint32_t& global_page_table_obj() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) global_page_table_mach_list()
       + PAGE_TABLE_MACH_LIST_SZ);
   return off;
}

uint32_t& global_page_table() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) global_page_table_obj()
       + PAGE_TABLE_OBJ_SZ);
   return off;
}

uint32_t& global_page_table_alloc_heap() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) global_page_table()
       + PAGE_TABLE_SZ);
   return off;
}

uint32_t& global_page_table_heap() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) global_page_table_alloc_heap()
       + PAGE_TABLE_ALLOC_HEAP_SZ);
   return off;
}

uint32_t& global_application_heap() {
   static uint32_t off = (uint32_t)
      ((uint8_t*) global_page_table_heap()
       + PAGE_TABLE_HEAP_SZ
       // TODO: remove this (it's for debugging)
       + (PAGE_SZ + 0xA0000000));
   return off;
}

uint32_t& global_pt_start_addr() {
   /**
    * Return the first address where the "page table region" starts.
    *
    */
   return global_page_table_mach_list();
}
