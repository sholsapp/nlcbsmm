#include <unistd.h>

#include "constants.h"

/**
 * Offsets
 */

uint32_t& global_base() {
   static uint32_t base = (uint32_t) sbrk(0);
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
