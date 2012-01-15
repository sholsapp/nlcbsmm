#include <cstdio>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

// Order matters
#include "vmmanager.h"
#include "hoard.h"
#include "packets.h"

#include <vector>

namespace NLCBSMM {
   /**
    * If we need memory to use for hoard, this is where we get it.
    */
   FreelistHeap<MmapHeap> myheap;

}

#define PAGE_SIZE 4096

using namespace Hoard;

using namespace HL;

namespace NLCBSMM {

   /**
    * This is supposed to be the "page table"
    */
   std::vector<SBEntry*, HoardAllocator<SBEntry* > > metadata_vector;


   class NetworkManager {

      public:

         NetworkManager(){
            /**
             * Constructor
             */
            threadid = 0;
         }

         ~NetworkManager(){
            /**
             * Destructor
             */
            int pid = 0;
            if((pid = waitpid(threadid, NULL,  __WCLONE)) == -1) {
               fprintf(stderr, "Return from wait (pid = %d)\n", pid);
               perror("wait error");
            }
         }

         void spawn_listener(void* stack, int size){
            /**
             * Spawns the listener thread
             *
             * stack: stack pointer
             * size: amount of memory valid on the stack
             */
            void* child_stack  = NULL;
            void* argument     = NULL;

            child_stack = (void*) (((int) stack) + size);
            argument    = (void*) 5;

            if((threadid = clone(&nlcbsmm_listener, child_stack, CLONE_VM | CLONE_FILES, argument)) == -1) {
               perror("vmmanager.cpp, clone");
               exit(EXIT_FAILURE);
            }
            return;
         }


         static int nlcbsmm_listener(void* t){
            /**
             *
             */
            int                 sk    = socket(AF_INET, SOCK_DGRAM, 0);
            socklen_t           len   = sizeof(struct sockaddr_in);
            struct sockaddr_in  local = {0};
            //memset(&local, 0, sizeof(struct sockaddr_in));
            local.sin_family          = AF_INET;
            local.sin_addr.s_addr     = INADDR_ANY;
            local.sin_port            = htons(6666);

            /* bind the name (address) to a port */
            if (bind(sk, (struct sockaddr *) &local, sizeof(local)) < 0) {
               perror("vmmanager.cpp, bind");
               exit(EXIT_FAILURE);
            }

            //get the port name and print it out
            if (getsockname(sk, (struct sockaddr*)&local, &len) < 0) {
               perror("vmmanager.cpp, getsockname");
               exit(EXIT_FAILURE);
            }

            printf("Thread socket has port %d \n", ntohs(local.sin_port));

            do_work(sk);

            close(sk);
            return 0;
         }

         static void do_work(int sk) {
            /**
             *
             */
            int buflen = PAGE_SIZE + sizeof(int) * 2;
            int done = 0;
            int read = 0;
            int sent = 0;
            char buf[buflen];
            sockaddr_in from;
            socklen_t fromLen = sizeof(from);

            struct NLCBSMMpacket* packet;

            memset(buf, 0, buflen);

            while(!done) {
               read = recvfrom(sk, buf, buflen, 0, (struct sockaddr*)&from, &fromLen);
               if(read) {
                  //fprintf(stderr,"Recived %d byes of data on the socket!\n", read);
                  packet = (struct NLCBSMMpacket*) &buf;
                  unsigned char type = packet->type;//only 1 byte, no endianness
                  unsigned int page = ntohl(packet->page); //handle endianness
                  //fprintf(stderr,"    type->%d\n", (int)type);


                  if(type == NLCBSMM_INVALIDATE) {//invalidate a page
                     //fprintf(stderr,"    Recieved shadow page command\n");
                     // Save the page in a shadow page (this is proof of concept code)
                     shadowPage((void*) page);
                  } else if(type == NLCBSMM_EXIT) {
                     //Exit the loop and clean up the socket
                     //fprintf(stderr,"    Recived kill command\n");
                     done = 1;
                  } else if(type == NLCBSMM_INIT) {
                     //fprintf(stderr,"    Recived init command\n");

                     //figure out header structure, linked list?
                     //send all sb entries
                     struct NLCBSMMpacket_init_response* ipacket = (struct NLCBSMMpacket_init_response*)buf;
                     ipacket->type = NLCBSMM_INIT_RESPONSE;
                     //fprintf(stderr,"    Setup type\n");
                     ipacket->length = metadata_vector.size();
                     //fprintf(stderr,"    Linked list size %d\n", ipacket->length);
                     int index = 0;

                     while(index < ipacket->length){
                        SBEntry* entry = metadata_vector[index];
                        ipacket->sb_addrs[index] = (unsigned int)entry->sb;
                        //fprintf(stderr, "index= %d, sb addr= %p\n", index, entry->sb);
                        index++;
                     }
                     //fprintf(stderr,"    Sent init response\n");
                     sent =  sendto(sk, buf, sizeof(struct NLCBSMMpacket_init_response), 0,(struct sockaddr*) &from, fromLen);

                  } else if(type == NLCBSMM_COR_PAGE) {
                     //get lock on metadata
                     unsigned int corpage = packet->page;
                     //fprintf(stderr,"    Copy on read recieved for page %p\n", (void*)corpage);
                     copyOnRead((void*)corpage, (struct sockaddr_in)from);
                     //unlock metadata
                  } else if(NLCBSMM_SEND_PAGE) {
                     //copy the page to a buffer and send it back


                  }
                  //clean up the buffer
                  memset(buf,0,buflen);
               }
            }
            return;
         }

         static void copyOnRead(void* page, struct sockaddr_in remote) {
            /**
             * COR a page.
             * First turn off permisions.
             * Then turn the MemoryLocation into NetworkLocation
             */
            NetworkLocation* netpage;

            //Page aligned page
            void * aligned = pageAlign((unsigned char*)page);

            // Try to look up the superblock entry
            SBEntry* entry = NULL; //metadata_vector.findSuperblock((void*) page);

            // If there is an entry
            if (entry) {
               // Get the index of this page in the superblock (i.e. 0-15)
               int whichPage = pageIndex((unsigned char*) page, (unsigned char*)entry->sb);

               //fprintf(stderr, "Operating on page %d\n", whichPage);

               // Allocate space for a new NetworkLocation
               void* space = myheap.malloc(sizeof(NetworkLocation));

               // Instantiate location
               netpage = new (space) NetworkLocation(remote);

               // Send this page to shadow memory
               //shadow->setPage(aligned);

               // Set no permissions on page
               if (mprotect(aligned, PAGESIZE, PROT_NONE)) {
                  perror("mprotect failure in copyOnRead");
                  exit(EXIT_FAILURE);
               }
               else {
                  // Free the reference to the old location
                  myheap.free(entry->page[whichPage].getLocation());

                  // Save the reference to the Location object in the superblock entry
                  entry->page[whichPage].setLocation(netpage);
                  //fprintf(stderr, "Networkified page(%p) and saving reference to network.\n", page);
               }

            }
            // Otherwise error
            else {
               fprintf(stderr, "Can't networkify page(%p): no SBEntry present.\n", page);
               exit(EXIT_FAILURE);
            }
            return;
         }


         static void shadowPage(void* page) {

            // A shadow memory page
            ShadowMemoryLocation* shadow;

            //Page aligned page
            void * aligned = pageAlign((unsigned char*)page);

            // Try to look up the superblock entry
            SBEntry* entry = NULL; //metadata.findSuperblock((void*) page);

            // If there is an entry
            if (entry) {
               // Get the index of this page in the superblock (i.e. 0-15)
               int whichPage = pageIndex((unsigned char*) page, (unsigned char*)entry->sb);

               //fprintf(stderr, "Operating on page %d\n", whichPage);

               // Allocate space for a new ShadowMemoryLocation
               //void* space = mm.allocObj(sizeof(ShadowMemoryLocation));
               void* space = myheap.malloc(sizeof(ShadowMemoryLocation));

               // Instantiate shadow memory
               shadow = new (space) ShadowMemoryLocation();

               // Send this page to shadow memory
               shadow->setPage(aligned);

               // Set no permissions on page
               if (mprotect(aligned, PAGESIZE, PROT_NONE)) {
                  perror("mprotect failure in shadowPage");
                  exit(EXIT_FAILURE);
               }
               else {
                  // Free the reference to the old location
                  myheap.free(entry->page[whichPage].getLocation());
                  //fprintf(stderr, "Freeing & ");



                  // Save the reference to the Location object in the superblock entry
                  entry->page[whichPage].setLocation(shadow);
                  //fprintf(stderr, "Corrupted page(%p) and saving reference to shadow.\n", page);
               }

            }
            // Otherwise error
            else {
               //fprintf(stderr, "Can't shadow page(%p): no SBEntry present.\n", page);
               exit(1);
            }
            return;
         }

      private:
         int threadid;

         static unsigned char* pageAlign(unsigned char* p) {
            /**
             * Helper function to page align a pointer
             */
            return (unsigned char *) (((int)p) & ~(PAGESIZE-1));
         }

         static unsigned int pageIndex(unsigned char* p, unsigned char* base) {
            /**
             * Help function to return page number in superblock
             */
            unsigned char* pageAligned = pageAlign(p);
            return (pageAligned - base) % PAGESIZE;
         }

   };

   /**
    * This is the distributed system server
    */
   NetworkManager networkmanager;


   unsigned char* pageAlign(unsigned char* p) {
      /**
       * Helper function to page align a pointer
       */
      return (unsigned char *)(((int)p) & ~(PAGESIZE-1));
   }

   unsigned int pageIndex(unsigned char* p, unsigned char* base) {
      /**
       * Help function to return page number in superblock
       */
      unsigned char* pageAligned = pageAlign(p);
      return (pageAligned - base) % PAGESIZE;
   }

   void signal_handler(int signo, siginfo_t* info, void* contex) {
      /**
       * The actual signal handler for SIGSEGV
       */

      //sigset_t oset;
      //sigset_t set;

      //block SIGSEGV
      //sigemptyset(&set);
      //sigaddset(&set, SIGSEGV);
      //sigprocmask(SIG_BLOCK, &set, &oset);

      //fprintf(stderr, "SIGSEGV Caught\n");

      unsigned char* p = pageAlign((unsigned char*) info->si_addr);
      //fprintf(stderr, "Illegal access at %p in page %p\n", info->si_addr, p);

      //SBEntry* entry = metadata.findSuperblock((void*) p);
      //if (entry) {

      // Find out what page in the entry faulted
      //int whichPage = pageIndex((unsigned char*)p, (unsigned char*)entry->sb);
      //fprintf(stderr,"Found the page\n");
      // Set the permissions on the page
      if (mprotect(p, PAGESIZE, PROT_READ | PROT_WRITE)) {
         perror("vmmanager.h: mprotect");
         exit(EXIT_FAILURE);
      }
      //else {
      //    fprintf(stderr, "Set PROT_READ | PROT_WRITE on %p\n", p);
      //}

      // Lookup where it is (by its Location class)
      // Copy the data into the real page
      //void* actualPage = entry->page[whichPage].getLocation()->getPage();
      //memcpy(p, actualPage, PAGESIZE);

      // Debug
      //fprintf(stderr, "Fault Resolved\n");
      //}
      //else {
      //    fprintf(stderr, ">> Seg Fault << \n");
      //    exit(1);
      //}

      //unblock sigsegv
      //sigprocmask(SIG_UNBLOCK, &set, &oset);
   }

   void register_signal_handlers() {
      /**
       * Registers signal handler for SIGSEGV
       */
      fprintf(stderr, "Registering SIGSEGV handler...");
      struct sigaction act;
      act.sa_sigaction = signal_handler;
      act.sa_flags     = SA_SIGINFO;
      sigemptyset(&act.sa_mask);
      sigaction(SIGSEGV, &act, 0);
      fprintf(stderr, "done\n");
      return;
   }

   void nlcbsmm_init() {
      /**
       * Hook entry.
       */
      int   stack_sz  = 4096;
      void* stack_ptr = NULL;

      register_signal_handlers();

      // Spawn the thread that listens for network commands
      stack_ptr = mmap(NULL, stack_sz , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      networkmanager.spawn_listener(stack_ptr, stack_sz);
      return;
   }
}
