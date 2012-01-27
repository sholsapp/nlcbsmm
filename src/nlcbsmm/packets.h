#ifndef __PACKETS_H_
#define __PACKETS_H_

#include <arpa/inet.h>
#include <netinet/in.h>

#define MULTICAST_JOIN_F 0xFF
#define UNICAST_JOIN_ACCEPT_F 0xFE
#define MULTICAST_HEARTBEAT_F 0x00

#include "constants.h"


class Packet {
   /**
    *
    */
   public:
      uint32_t get_sequence() {
         return ntohl(((uint32_t*) this)[0]);
      }

      uint32_t get_payload_sz() {
        return ntohl(((uint32_t*) this)[1]);
      }

      uint8_t get_flag() {
         return ((uint8_t*) this)[8];
      }

      uint8_t* get_payload_ptr() {
         return &(((uint8_t*) this)[PACKET_HEADER_SZ]);
      }
};


class MulticastHeartbeat : public Packet {
   /**
    * When a multicast thread has no work, its heart beats.
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint32_t flags;

      MulticastHeartbeat(uint32_t user_length) {
         sequence   = htonl(0);
         payload_sz = htonl(user_length);
         flags      = MULTICAST_HEARTBEAT_F;
      }

};

class MulticastJoin : public Packet {
   /**
    * When a node starts, this is the first packet multicasted to the group.  It
    * is an attempt to synchronize with n or more nodes in the cluster (or none
    * if this node is the first (e.g., master node)).
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t main_addr;
      uint32_t end_addr;
      uint32_t data_start_addr;

      MulticastJoin(uint32_t user_length, uint8_t** _main_addr, uint8_t** _end_addr, uint8_t** __data_start_addr) {
         /**
          *
          */
         sequence        = htonl(0);
         payload_sz      = htonl(user_length);
         flag            = MULTICAST_JOIN_F;
         main_addr       = htonl(reinterpret_cast<uint32_t>(_main_addr));
         end_addr        = htonl(reinterpret_cast<uint32_t>(_end_addr));
         data_start_addr = htonl(reinterpret_cast<uint32_t>(__data_start_addr));
      }
}__attribute__((packed));


class UnicastJoinAcceptance : public Packet {
   /**
    * When a requests to join the group, the master node sends this directly to
    * the node requesting to join.  This packet indicates the node is being added
    * to the cluster, and assigns the new node a unique identifier.
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t start_page_table;
      uint32_t end_page_table;
      uint32_t uuid;

      UnicastJoinAcceptance(uint32_t user_length, uint32_t _start_pt, uint32_t _end_pt, uint32_t _uuid) {

         sequence          = htonl(0);
         payload_sz        = htonl(user_length);
         flag              = UNICAST_JOIN_ACCEPT_F;
         start_page_table  = htonl(_start_pt);
         end_page_table    = htonl(_end_pt);
         uuid              = htonl(_uuid);
      }

}__attribute__((packed));

#endif
