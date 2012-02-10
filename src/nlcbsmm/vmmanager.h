/**
 * NLCBSMM Virtual Memory Manager
 */
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <vector>
#include <deque>

#include "page.h"
#include "packets.h"
#include "cln_allocator.h"
#include "pt_allocator.h"

namespace NLCBSMM {

   // A vector of NLCBSMM::Page objects
   typedef std::vector<Page*,
           PageTableAllocator<Page*> > PageVectorType;

   typedef PageVectorType::iterator PageVectorItr;

   // A mapping of 'ip address' to a NLCBSMM::PageVector, where 'ip address' is the
   // binary representation of the IPv4 address in dot-notation.
   typedef std::map<uint32_t,
           PageVectorType*,
           std::less<uint32_t>,
           PageTableAllocator<std::pair<uint32_t, PageVectorType*> > > PageTableType;

   typedef PageTableType::iterator PageTableItr;

   // Who to contact, and with what
   typedef std::pair<struct sockaddr_in, Packet*> WorkTupleType;

   // A queue of work
   typedef std::deque<WorkTupleType*,
           CloneAllocator<WorkTupleType* > > PacketQueueType;
}

namespace NLCBSMM {

   // The node's IP address
   extern const char* local_ip;

   // The page table
   extern PageTableType* page_table;

   // Helper function to page align a pointer
   unsigned char* pageAlign(unsigned char* p);

   // Helper function to return page number in superblock
   unsigned int pageIndex(unsigned char* p, unsigned char* base);

   // The actual signal handler for SIGSEGV
   void signal_handler(int signo, siginfo_t* info, void* contex);

   // Registers signal handler for SIGSEGV
   void register_signal_handlers();

   // Initialize the NLCBSMM system
   void nlcbsmm_init();

}
