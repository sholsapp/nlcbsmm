#ifndef __PACKETS_H_
#define __PACKETS_H_

#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>

#define MULTICAST_JOIN_F          0x0A
#define MULTICAST_HEARTBEAT_F     0x0B
#define MULTICAST_OWNER_UPDATE_F  0x0C

#define UNICAST_JOIN_ACCEPT_F     0x1A
#define UNICAST_JOIN_ACCEPT_ACK_F 0x1B

#define SYNC_START_F              0x20
#define SYNC_START_ACK_F          0x21
#define SYNC_ACQUIRE_PAGE_F       0x23
#define SYNC_RELEASE_PAGE_F       0x24
#define SYNC_RELEASE_PAGE_ACK_F   0x25
#define SYNC_PAGE_F               0x2A
#define SYNC_PAGE_ACK_F           0x2B
#define SYNC_DONE_F               0x2C
#define SYNC_DONE_ACK_F           0x2D
#define SYNC_RESERVE_F            0x2E

#define THREAD_CREATE_F           0x3A
#define THREAD_CREATE_ACK_F       0x3B
#define THREAD_JOIN_F             0x3C
#define THREAD_JOIN_ACK_F         0x3D

#define ACQUIRE_WRITE_LOCK_F      0x4A
#define RELEASE_WRITE_LOCK_F      0x4B

#define MUTEX_LOCK_REQUEST_F      0x5A
#define MUTEX_LOCK_GRANT_F        0x5B
#define MUTEX_UNLOCK_F            0x5C

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

      void set_flag(uint8_t new_flag) {
         uint8_t old_flag = get_flag();
         ((uint8_t*) this)[8] = new_flag;
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

}__attribute__((packed));

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

      uint32_t ip_address;
      uint32_t main_addr;
      uint32_t end_addr;
      uint32_t prog_break_addr;

      MulticastJoin(uint32_t _ip_address, 
            uint8_t* _main_addr, 
            uint8_t* _end_addr, 
            uint8_t* _prog_break_addr) {
         /**
          *
          */
         sequence        = htonl(0);
         payload_sz      = htonl(0);
         flag            = MULTICAST_JOIN_F;
         ip_address      = htonl(_ip_address);
         main_addr       = htonl(reinterpret_cast<uint32_t>(_main_addr));
         end_addr        = htonl(reinterpret_cast<uint32_t>(_end_addr));
         prog_break_addr = htonl(reinterpret_cast<uint32_t>(_prog_break_addr));
      }
}__attribute__((packed));


class OwnerUpdate : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint32_t flags;

      uint32_t ip;
      uint32_t page_addr;

      OwnerUpdate(uint32_t _ip, uint32_t _page_addr) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flags      = MULTICAST_OWNER_UPDATE_F;
         ip         = htonl(_ip);
         page_addr  = htonl(_page_addr);
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
      uint32_t pt_owner;

      UnicastJoinAcceptance() {}

      UnicastJoinAcceptance(void* copy) {
         // Copy data from copy into this class
         memcpy(this, copy, MAX_PACKET_SZ);
         // Set the flag appropriately (this is to be used as an ACK)
         this->set_flag(UNICAST_JOIN_ACCEPT_ACK_F);
      }

      UnicastJoinAcceptance(uint32_t _user_length,
            uint32_t _start_pt, 
            uint32_t _end_pt, 
            uint32_t _uuid, 
            uint32_t _pt_owner) {

         sequence          = htonl(0);
         payload_sz        = htonl(_user_length);
         flag              = UNICAST_JOIN_ACCEPT_F;
         start_page_table  = htonl(_start_pt);
         end_page_table    = htonl(_end_pt);
         uuid              = htonl(_uuid);
         pt_owner          = htonl(_pt_owner);
      }

}__attribute__((packed));


class SyncPage : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t page_offset;

      SyncPage(uint32_t page_addr, void* page_data) {
         sequence    = htonl(0);
         payload_sz  = htonl(PAGE_SZ);
         flag        = SYNC_PAGE_F;
         page_offset = htonl(page_addr);
         // Copy payload into packet
         memcpy(this->get_payload_ptr(), page_data, MAX_PACKET_SZ);
      }

}__attribute__((packed));


class SyncReserve : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t ip;
      uint32_t start_addr;
      uint32_t sz;

      SyncReserve(uint32_t _owner, void* _start_addr, uint32_t _sz) {
         sequence    = htonl(0);
         payload_sz  = htonl(0);
         flag        = SYNC_RESERVE_F;

         ip          = htonl(_owner);
         start_addr  = htonl(reinterpret_cast<uint32_t>(_start_addr));
         sz          = htonl(_sz);
      }

}__attribute__((packed));


class ThreadCreate : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t stack_ptr;
      uint32_t stack_sz;

      uint32_t func_ptr;
      uint32_t arg;

      ThreadCreate(void* _stack_ptr, void* _func_ptr, void* _arg) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = THREAD_CREATE_F;
         stack_ptr  = htonl(reinterpret_cast<uint32_t>(_stack_ptr));
         stack_sz   = htonl(PTHREAD_STACK_SZ);
         func_ptr   = htonl(reinterpret_cast<uint32_t>(_func_ptr));
         arg        = htonl(reinterpret_cast<uint32_t>(_arg));
      }

}__attribute__((packed));


class ThreadCreateAck : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t thread_id;

      ThreadCreateAck(uint32_t _thread_id) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = THREAD_CREATE_ACK_F;
         thread_id  = htonl(_thread_id);
      }

}__attribute__((packed));


class ThreadJoin : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      ThreadJoin() {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = THREAD_JOIN_F;
      }

}__attribute__((packed));


class AcquireWriteLock : public Packet {
   /**
    * This is used to request write access to the page table.
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint16_t ret_port;

      AcquireWriteLock(uint16_t _ret_port) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = ACQUIRE_WRITE_LOCK_F;
         ret_port   = htons(_ret_port);
      }

}__attribute__((packed));


class ReleaseWriteLock : public Packet {
   /**
    * This is used to release write access to the page table.
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;
      uint32_t next_addr;

      ReleaseWriteLock(uint32_t _next_addr) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = RELEASE_WRITE_LOCK_F;
         next_addr  = htonl(_next_addr);
      }

}__attribute__((packed));


class AcquirePage : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t page_addr;

      AcquirePage(uint32_t _page_addr) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = SYNC_ACQUIRE_PAGE_F;
         page_addr  = htonl(_page_addr);
      }


}__attribute__((packed));


class ReleasePage : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t page_addr;

      ReleasePage(uint32_t _page_addr) {
         sequence   = htonl(0);
         payload_sz = htonl(PAGE_SZ);
         flag       = SYNC_RELEASE_PAGE_F;
         page_addr  = htonl(_page_addr);
         // Copy payload into packet
         memcpy(this->get_payload_ptr(), (void*) _page_addr, PAGE_SZ);
      }

}__attribute__((packed));

class MutexLockRequest : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t mutex_id;

      MutexLockRequest(uint32_t _mutex_id) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = MUTEX_LOCK_REQUEST_F;
         mutex_id   = htonl(_mutex_id);
      }

}__attribute__((packed));

class MutexLockGrant : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t mutex_id;

      MutexLockGrant(uint32_t _mutex_id) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = MUTEX_LOCK_GRANT_F;
         mutex_id   = htonl(_mutex_id);
      }

}__attribute__((packed));

class MutexUnlock : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      uint32_t mutex_id;

      MutexUnlock(uint32_t _mutex_id) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = MUTEX_UNLOCK_F;
         mutex_id   = htonl(_mutex_id);
      }

}__attribute__((packed));


class GenericPacket : public Packet {
   /**
    *
    */
   public:
      uint32_t sequence;
      uint32_t payload_sz;
      uint8_t  flag;

      GenericPacket(uint8_t type) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = type;
      }

}__attribute__((packed));

#endif
