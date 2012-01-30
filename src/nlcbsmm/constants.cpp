#include <unistd.h>
#include <netinet/in.h> // for types

#define PAGE_SIZE 4096

/**
 * Virtual memory constants
 */
uint32_t     CLONE_STACK_SZ            = 4096 * 64;
uint32_t     PAGE_TABLE_SZ             = 4096 * 16;
uint32_t     CLONE_HEAP_SZ             = 4096 * 512;
uint32_t     PAGE_TABLE_ALLOC_HEAP_SZ  = 4096 * 16;
uint32_t     PAGE_TABLE_HEAP_SZ        = 4096 * 512;

/**
 * Networking constants
 */

uint16_t     UNICAST_PORT           = 60000;
uint16_t     MULTICAST_PORT         = 60001;
const char*  MULTICAST_GRP          = "225.0.0.6";

uint32_t     PACKET_HEADER_SZ       = 256;
uint32_t     PACKET_MAX_PAYLOAD_SZ  = 8192;

uint32_t     MAX_PACKET_SZ          = PACKET_HEADER_SZ + PACKET_MAX_PAYLOAD_SZ;
