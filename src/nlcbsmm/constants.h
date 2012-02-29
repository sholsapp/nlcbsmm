#include <unistd.h>
#include <netinet/in.h>

#define MAX_JOIN_ATTEMPTS 3

#define CLONE_ATTRS (CLONE_VM | CLONE_FILES | CLONE_SIGHAND | CLONE_PTRACE)
#define CLONE_MMAP_PROT_FLAGS (PROT_READ | PROT_WRITE)
#define CLONE_MMAP_FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)

#define PAGE_SZ                       4096
#define PTHREAD_STACK_SZ             (4096 * 32)

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

#define WORKER_STACK_SZ              (4096 * 128 )
#define CLONE_STACK_SZ               (4096 * 128 )
#define CLONE_ALLOC_HEAP_SZ          (4096 * 128 )
#define CLONE_HEAP_SZ                (4096 * 4096)

#define PAGE_TABLE_MACH_LIST_SZ      (4096 * 128 )
#define PAGE_TABLE_OBJ_SZ            (4096 * 128 )
#define PAGE_TABLE_SZ                (4096 * 128 )
#define PAGE_TABLE_ALLOC_HEAP_SZ     (4096 * 128 )
#define PAGE_TABLE_HEAP_SZ           (4096 * 4096)

/**
 * Networking constants
 */

#define UNICAST_PORT                 60000
#define MULTICAST_PORT               60001
#define MULTICAST_GRP                "225.0.0.6"

#define PACKET_HEADER_SZ             256
#define PACKET_MAX_PAYLOAD_SZ        8192
#define MAX_PACKET_SZ                (256 + 8192)

#define MACHINE_INIT    0
#define MACHINE_SYNC    1
#define MACHINE_IDLE    2
#define MACHINE_ACTIVE  3
#define MACHINE_MIA     4
#define MACHINE_MASTER  5

/**
 *
 */
extern uint32_t& global_main();
extern uint32_t& global_end();
extern uint32_t& global_base();
extern uint32_t& global_clone_alloc_heap();
extern uint32_t& global_clone_heap();
extern uint32_t& global_page_table_mach_list();
extern uint32_t& global_page_table_obj();
extern uint32_t& global_page_table();
extern uint32_t& global_page_table_alloc_heap();
extern uint32_t& global_page_table_heap();
extern uint32_t& global_application_heap();
extern uint32_t& global_pt_start_addr();

extern uint32_t& get_workers_fixed_addr();
extern uint32_t& get_clonebase_fixed_addr();
