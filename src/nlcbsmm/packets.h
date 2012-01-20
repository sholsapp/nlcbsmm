#ifndef __PACKETS_H_
#define __PACKETS_H_

#include <arpa/inet.h>
#include <netinet/in.h>

#define MULTICAST_JOIN_F 0xFF

class Packet {
   /**
    *
    */
   public:
      uint32_t get_sequence() {
         return ntohl(((uint32_t*) this)[0]);
      }

      uint8_t get_flag() {
         return ((uint8_t*) this)[4];
      }
};

class MulticastJoin : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint8_t  flag;

      uint32_t main_addr;
      uint32_t init_addr;
      uint32_t fini_addr;
      uint32_t end_addr;
      uint32_t data_start_addr;

      uint32_t payload_sz;

      MulticastJoin(uint8_t** _main_addr, uint8_t** _init_addr, uint8_t** _fini_addr, uint8_t** _end_addr, uint8_t** __data_start_addr) {
         /**
          *
          */
         sequence        = htonl(0);
         flag            = MULTICAST_JOIN_F;
         main_addr       = htonl(reinterpret_cast<uint32_t>(_main_addr));
         init_addr       = htonl(reinterpret_cast<uint32_t>(_init_addr));
         fini_addr       = htonl(reinterpret_cast<uint32_t>(_fini_addr));
         end_addr        = htonl(reinterpret_cast<uint32_t>(_end_addr));
         data_start_addr = htonl(reinterpret_cast<uint32_t>(__data_start_addr));
         payload_sz      = htonl(0);
      }

}__attribute__((packed));

#endif
