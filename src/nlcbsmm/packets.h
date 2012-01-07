#ifndef __PACKETS_H_
#define __PACKETS_H_

#define NLCBSMM_INVALIDATE 0x0
#define NLCBSMM_EXIT 0x1
#define NLCBSMM_INIT 0x2
#define NLCBSMM_INIT_RESPONSE 0x3
#define NLCBSMM_COR_PAGE 0x4
#define NLCBSMM_COR_PAGE_RESPONSE 0x5
// Sent as a request for a page
#define NLCBSMM_GET_PAGE 0x6
// Received as answer to 'get' data
#define NLCBSMM_SEND_PAGE 0x7

struct NLCBSMMpacket {
    unsigned char type;
    unsigned int page;
} __attribute__((packed));


struct NLCBSMMpacket_init {
    unsigned int type;
} __attribute__((packed));

struct NLCBSMMpacket_init_response {
    unsigned char type;
    unsigned int length;
    unsigned int sb_addrs[1337];//FIX ME PLS
} __attribute__((packed));


/**
 * Used in location.h
 */
struct NLCBSMMpacket_getpage {
    unsigned char type;
    unsigned int baseaddr;
}__attribute__((packed)); 
typedef struct NLCBSMMpacket_getpage GetPagePacket;

struct NLCBSMMpacket_sendpage {
    unsigned char type;
    unsigned int addr;
    unsigned char page[4096]; 
}__attribute__((packed));
typedef struct NLCBSMMpacket_sendpage SendPagePacket;

#endif
