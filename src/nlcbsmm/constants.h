
#define PAGE_SIZE 4096
#define MAX_JOIN_ATTEMPTS 5


/**
 * Allocator flags to determine fixed region of memory to
 * allocate data in.
 */
#define PRIVATE_STACK_START         6666;
#define PRIVATE_HEAP_START          6667;
#define PAGE_TABLE_START            6668;
#define PAGE_TABLE_ALLOC_HEAP_START 6669;
#define PAGE_TABLE_HEAP_START       6670;

/**
 * Virtual memory constants
 */
extern uint32_t     CLONE_STACK_SZ;
extern uint32_t     PAGE_TABLE_SZ;

extern uint32_t     CLONE_HEAP_SZ;
extern uint32_t     PAGE_TABLE_ALLOC_HEAP_SZ;
extern uint32_t     PAGE_TABLE_HEAP_SZ;

/**
 * Networking constants
 */

extern uint16_t     UNICAST_PORT;
extern uint16_t     MULTICAST_PORT;
extern const char*  MULTICAST_GRP;

extern uint32_t     PACKET_HEADER_SZ;
extern uint32_t     PACKET_MAX_PAYLOAD_SZ;
extern uint32_t     MAX_PACKET_SZ;
