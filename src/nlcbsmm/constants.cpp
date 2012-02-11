#include <cstdio>

#include "constants.h"

/**
 * Offsets
 */

extern uint8_t* _end;

uint32_t& global_base() {
   // Page align the _end of the program + a page
   static uint32_t base = (((uint32_t) &_end) & ~(PAGE_SZ - 1)) + PAGE_SZ;
   return base;
}

uint32_t& global_clone_alloc_heap() {
   static uint32_t off = (uint32_t) ((uint8_t*) global_base() + (CLONE_STACK_SZ * 4));
   return off;
}

uint32_t& global_clone_heap() {
   static uint32_t off = (uint32_t) ((uint8_t*) global_clone_alloc_heap() + (CLONE_ALLOC_HEAP_SZ));
   return off;
}

uint32_t& global_page_table_obj() {
  static uint32_t off = (uint32_t) ((uint8_t*) global_clone_heap() + (CLONE_HEAP_SZ));
}

uint32_t& global_page_table() {
   static uint32_t off = (uint32_t) ((uint8_t*) global_page_table_obj() + (PAGE_TABLE_OBJ_SZ));
   return off;
}

uint32_t& global_page_table_alloc_heap() {
   static uint32_t off = (uint32_t) ((uint8_t*) global_page_table() + (PAGE_TABLE_SZ));
   return off;
}

uint32_t& global_page_table_heap() {
   static uint32_t off = (uint32_t) ((uint8_t*) global_page_table_alloc_heap() + (PAGE_TABLE_ALLOC_HEAP_SZ));
   return off;
}
