#include <unistd.h>
#include <netinet/in.h>

#define MAX_JOIN_ATTEMPTS 5

#define PAGE_SZ           4096

/**
 * Allocator flags to determine fixed region of memory to
 * allocate data in.
 *
 * These are #define'd so we can use them in class template
 * parameters.
 */
#define CLONE_STACK_START            100
#define PAGE_TABLE_START             101

// These values must be exactly one value apart
//  (see nlcbsmmheap.h)
#define CLONE_HEAP_START             102
#define CLONE_ALLOC_HEAP_START       103

// These values must be exactly one value apart
//  (see nlcbsmmheap.h)
#define PAGE_TABLE_HEAP_START        104
#define PAGE_TABLE_ALLOC_HEAP_START  105

#define CLONE_STACK_SZ (4096 * 64)
#define PAGE_TABLE_SZ (4096 * 16)

#define CLONE_ALLOC_HEAP_SZ (4096 * 16)
#define CLONE_HEAP_SZ (4096 * 512)

#define PAGE_TABLE_ALLOC_HEAP_SZ (4096 * 16)
#define PAGE_TABLE_HEAP_SZ (4096 * 512)

/**
 * Virtual memory constants
 */
/*
extern uint32_t     BASE;
extern uint32_t     CLONE_ALLOC_HEAP_OFFSET;
extern uint32_t     CLONE_HEAP_OFFSET;

extern uint32_t     PAGE_TABLE_OFFSET;
extern uint32_t     PAGE_TABLE_ALLOC_HEAP_OFFSET;
extern uint32_t     PAGE_TABLE_HEAP_OFFSET;
*/

/**
 * Networking constants
 */

#define UNICAST_PORT 60000
#define MULTICAST_PORT 60001
#define MULTICAST_GRP "255.0.0.6"

#define PACKET_HEADER_SZ 256
#define PACKET_MAX_PAYLOAD_SZ 8192

#define MAX_PACKET_SZ (256 + 8192)

/**
 *
 */
extern uint32_t& global_base();
extern uint32_t& global_clone_alloc_heap();
extern uint32_t& global_clone_heap();
extern uint32_t& global_page_table();
extern uint32_t& global_page_table_alloc_heap();
extern uint32_t& global_page_table_heap();
