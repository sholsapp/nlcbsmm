#include <cstdio>

#include "constants.h"

/**
 * Offsets
 */

extern uint8_t* _end;

uint32_t& global_base() {
   uint32_t end = (uint32_t) &_end;
   uint32_t aligned = end & ~(4096-1);
   static uint32_t base = (uint32_t) aligned + 4096;
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

uint32_t& global_page_table() {
   static uint32_t off = (uint32_t) ((uint8_t*) global_clone_heap() + (CLONE_HEAP_SZ));
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
