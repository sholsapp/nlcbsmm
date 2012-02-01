#include "constants.h"

/**
 * Virtual memory constants
 */
uint32_t     CLONE_STACK_SZ            = PAGE_SZ * 64;
uint32_t     PAGE_TABLE_SZ             = PAGE_SZ * 16;

uint32_t     CLONE_ALLOC_HEAP_SZ       = PAGE_SZ * 16;
uint32_t     CLONE_HEAP_SZ             = PAGE_SZ * 512;

uint32_t     PAGE_TABLE_ALLOC_HEAP_SZ  = PAGE_SZ * 16;
uint32_t     PAGE_TABLE_HEAP_SZ        = PAGE_SZ * 512;

/**
 * Offsets
 */
uint32_t  BASE                         = (uint32_t) ((uint8_t*) sbrk(0));
uint32_t  CLONE_ALLOC_HEAP_OFFSET      = (uint32_t) ((uint8_t*) BASE  + (CLONE_STACK_SZ * 4));
uint32_t  CLONE_HEAP_OFFSET            = (uint32_t) ((uint8_t*) CLONE_ALLOC_HEAP_OFFSET + (CLONE_ALLOC_HEAP_SZ));
uint32_t  PAGE_TABLE_OFFSET            = (uint32_t) ((uint8_t*) CLONE_HEAP_OFFSET + (CLONE_HEAP_SZ));
uint32_t  PAGE_TABLE_ALLOC_HEAP_OFFSET = (uint32_t) ((uint8_t*) PAGE_TABLE_OFFSET + (PAGE_TABLE_SZ));
uint32_t  PAGE_TABLE_HEAP_OFFSET       = (uint32_t) ((uint8_t*) PAGE_TABLE_ALLOC_HEAP_OFFSET + (PAGE_TABLE_ALLOC_HEAP_SZ));

/**
 * Networking constants
 */

uint16_t     UNICAST_PORT           = 60000;
uint16_t     MULTICAST_PORT         = 60001;
const char*  MULTICAST_GRP          = "225.0.0.6";

uint32_t     PACKET_HEADER_SZ       = 256;
uint32_t     PACKET_MAX_PAYLOAD_SZ  = 8192;

uint32_t     MAX_PACKET_SZ          = PACKET_HEADER_SZ + PACKET_MAX_PAYLOAD_SZ;
