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
#define MUTEX_UNLOCK_ACK_F        0x5D

#include "constants.h"


class Packet {
   /**
    *
    */
   public:
      int get_sequence() {
         return ntohl(((int*) this)[0]);
      }

      int get_payload_sz() {
         return ntohl(((int*) this)[1]);
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
      int sequence;
      int payload_sz;
      int flags;

      MulticastHeartbeat(int user_length) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      int ip_address;

      intptr_t main_addr;
      intptr_t end_addr;
      intptr_t prog_break_addr;

      MulticastJoin(int _ip_address, 
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
         main_addr       = htonl(reinterpret_cast<intptr_t>(_main_addr));
         end_addr        = htonl(reinterpret_cast<intptr_t>(_end_addr));
         prog_break_addr = htonl(reinterpret_cast<intptr_t>(_prog_break_addr));
      }
}__attribute__((packed));


class OwnerUpdate : public Packet {
   /**
    *
    */
   public:
      int sequence;
      int payload_sz;
      int flags;

      int ip;
      intptr_t page_addr;

      OwnerUpdate(int _ip, intptr_t _page_addr) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t start_page_table;
      intptr_t end_page_table;
      int uuid;
      int pt_owner;

      UnicastJoinAcceptance() {}

      UnicastJoinAcceptance(void* copy) {
         // Copy data from copy into this class
         memcpy(this, copy, MAX_PACKET_SZ);
         // Set the flag appropriately (this is to be used as an ACK)
         this->set_flag(UNICAST_JOIN_ACCEPT_ACK_F);
      }

      UnicastJoinAcceptance(int _user_length,
            intptr_t _start_pt, 
            intptr_t _end_pt, 
            int _uuid, 
            int _pt_owner) {

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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t page_offset;

      SyncPage(intptr_t page_addr, void* page_data) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      int ip;
      intptr_t start_addr;
      int sz;

      SyncReserve(int _owner, void* _start_addr, int _sz) {
         sequence    = htonl(0);
         payload_sz  = htonl(0);
         flag        = SYNC_RESERVE_F;

         ip          = htonl(_owner);
         start_addr  = htonl(reinterpret_cast<intptr_t>(_start_addr));
         sz          = htonl(_sz);
      }

}__attribute__((packed));


class ThreadCreate : public Packet {
   /**
    *
    */
   public:
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t stack_ptr;
      int stack_sz;

      intptr_t func_ptr;
      intptr_t arg;

      ThreadCreate(void* _stack_ptr, void* _func_ptr, void* _arg) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = THREAD_CREATE_F;
         stack_ptr  = htonl(reinterpret_cast<intptr_t>(_stack_ptr));
         stack_sz   = htonl(PTHREAD_STACK_SZ);
         func_ptr   = htonl(reinterpret_cast<intptr_t>(_func_ptr));
         arg        = htonl(reinterpret_cast<intptr_t>(_arg));
      }

}__attribute__((packed));


class ThreadCreateAck : public Packet {
   /**
    *
    */
   public:
      int sequence;
      int payload_sz;
      uint8_t  flag;

      int thread_id;

      ThreadCreateAck(int _thread_id) {
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
      int sequence;
      int payload_sz;
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
      int sequence;
      int payload_sz;
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
      int sequence;
      int payload_sz;
      uint8_t  flag;
      intptr_t next_addr;

      ReleaseWriteLock(intptr_t _next_addr) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t page_addr;

      AcquirePage(intptr_t _page_addr) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t page_addr;

      ReleasePage(intptr_t _page_addr) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t mutex_id;

      MutexLockRequest(intptr_t _mutex_id) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t mutex_id;

      MutexLockGrant(intptr_t _mutex_id) {
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
      int sequence;
      int payload_sz;
      uint8_t  flag;

      intptr_t mutex_id;

      MutexUnlock(intptr_t _mutex_id, size_t _payload_sz, vmaddr_t* _bits) {
         sequence   = htonl(0);
         payload_sz = htonl(_payload_sz);
         flag       = MUTEX_UNLOCK_F;
         mutex_id   = htonl(_mutex_id);
         // Copy payload into packet
         memcpy(this->get_payload_ptr(), (void*) _bits, _payload_sz);
      }

}__attribute__((packed));


class GenericPacket : public Packet {
   /**
    *
    */
   public:
      int sequence;
      int payload_sz;
      uint8_t  flag;

      GenericPacket(uint8_t type) {
         sequence   = htonl(0);
         payload_sz = htonl(0);
         flag       = type;
      }

}__attribute__((packed));

#endif
