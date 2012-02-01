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

/**
 * Virtual memory constants
 */
extern uint32_t     CLONE_STACK_SZ;
extern uint32_t     PAGE_TABLE_SZ;

extern uint32_t     CLONE_ALLOC_HEAP_SZ;
extern uint32_t     CLONE_HEAP_SZ;
extern uint32_t     PAGE_TABLE_ALLOC_HEAP_SZ;
extern uint32_t     PAGE_TABLE_HEAP_SZ;

extern uint32_t     BASE;
extern uint32_t     CLONE_ALLOC_HEAP_OFFSET;
extern uint32_t     CLONE_HEAP_OFFSET;

extern uint32_t     PAGE_TABLE_OFFSET;
extern uint32_t     PAGE_TABLE_ALLOC_HEAP_OFFSET;
extern uint32_t     PAGE_TABLE_HEAP_OFFSET;

/**
 * Networking constants
 */

extern uint16_t     UNICAST_PORT;
extern uint16_t     MULTICAST_PORT;
extern const char*  MULTICAST_GRP;

extern uint32_t     PACKET_HEADER_SZ;
extern uint32_t     PACKET_MAX_PAYLOAD_SZ;
extern uint32_t     MAX_PACKET_SZ;
