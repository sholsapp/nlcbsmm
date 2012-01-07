#include <cstdio>
#include <iostream>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/wait.h>
#include "vmmanager.h"
#include "hoard.h"
#include "packets.h"

#define PAGE_SIZE 4096


using namespace Hoard;
using namespace HL;

namespace NLCBSMM {

   class NetworkManager{

      public:

         NetworkManager(){
            threadid=0;
         }

         ~NetworkManager(){
            //TODO thread cleanup code
            //fprintf(stderr, "NetworkManager decon: thread %d\n", getpid());
            errno = 0;
            //int pid = wait(NULL);
            int pid = waitpid(threadid, NULL,  __WCLONE);
            if(pid == -1){
               perror("wait error");
               fprintf(stderr, "Return from wait=%d\n",pid);
            }
         }

         /**
          * Spawns the listener thread
          * st: stack pointer
          * size: amount of memory valid on the stack
          */
         void spawn_listener(void* st, int size){
            int rc;
            long t = 5;

            rc = clone(&nlcbsmm_listener,(void*)(((int)st)+size), CLONE_VM | CLONE_FILES, (void*)t);
            if(rc == -1) {
               printf("ERROR; return code from clone is %d\n", rc);
               exit(-1);
            }
            else {
               //fprintf(stderr,"Spawned a listener thread (%d)!\n", rc);
            }
            threadid = rc;
            return;
         }

         static int nlcbsmm_listener(void* t){
            int sk = socket(AF_INET,SOCK_DGRAM,0);
            struct sockaddr_in local;
            socklen_t len = sizeof(struct sockaddr_in);

            //make sure it is empty
            memset(&local, 0, sizeof(struct sockaddr_in));
            local.sin_family = AF_INET;
            local.sin_addr.s_addr = INADDR_ANY;
            local.sin_port = htons(6666);

            /* bind the name (address) to a port */
            if (bind(sk, (struct sockaddr *) &local, sizeof(local)) < 0) {
               perror("bind call");
               exit(-1);
            }


            //get the port name and print it out
            if (getsockname(sk, (struct sockaddr*)&local, &len) < 0) {
               perror("getsockname call");
               exit(-1);
            }

            printf("Thread socket has port %d \n", ntohs(local.sin_port));

            do_work(sk);

            close(sk);
            //fprintf(stderr, "Network thread: socket now closed!\n");
            return 0;
         }

         static void do_work(int sk){
            sockaddr_in from;
            struct NLCBSMMpacket* packet;
            socklen_t fromLen = sizeof(from);
            int buflen = PAGE_SIZE + sizeof(int) * 2;
            char buf[buflen];
            int done = 0, read = 0, sent = 0;
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
                     ipacket->length = metadata.getSize();
                     //fprintf(stderr,"    Linked list size %d\n", ipacket->length);
                     int index = 0;

                     while(index < ipacket->length){
                        SBEntry* entry = metadata.get(index);
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

         /**
          * COR a page.
          * First turn off permisions.
          * Then turn the MemoryLocation into NetworkLocation
          */
         static void copyOnRead(void* page, struct sockaddr_in remote) {
            NetworkLocation* netpage;

            //Page aligned page
            void * aligned = pageAlign((unsigned char*)page);

            // Try to look up the superblock entry
            SBEntry* entry = metadata.findSuperblock((void*) page);

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
                  exit(errno);
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
               //fprintf(stderr, "Can't networkify page(%p): no SBEntry present.\n", page);
               exit(1);
            }
            return;
         }




         static void shadowPage(void* page) {

            // A shadow memory page
            ShadowMemoryLocation* shadow;

            //Page aligned page
            void * aligned = pageAlign((unsigned char*)page);

            // Try to look up the superblock entry
            SBEntry* entry = metadata.findSuperblock((void*) page);

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
                  exit(errno);
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

         /**
          * Helper function to page align a pointer
          */
         static unsigned char* pageAlign(unsigned char* p) {
            return (unsigned char *)(((int)p) & ~(PAGESIZE-1));
         }

         /**
          * Help function to return page number in superblock
          */
         static unsigned int pageIndex(unsigned char* p, unsigned char* base) {
            unsigned char* pageAligned = pageAlign(p);
            return (pageAligned - base) % PAGESIZE;
         }

   };

   /*
    * Global Data Section
    */
   FreelistHeap<MmapHeap> myheap;

   LinkedList metadata(&myheap);

   NetworkManager networkmanager;

   /**
    * Helper function to page align a pointer
    */
   unsigned char* pageAlign(unsigned char* p) {
      return (unsigned char *)(((int)p) & ~(PAGESIZE-1));
   }

   /**
    * Help function to return page number in superblock
    */
   unsigned int pageIndex(unsigned char* p, unsigned char* base) {
      unsigned char* pageAligned = pageAlign(p);
      return (pageAligned - base) % PAGESIZE;
   }

   /**
    * The actual signal handler for SIGSEGV
    */
   void signal_handler(int signo, siginfo_t* info, void* contex) {
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
         exit(errno);
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

   /**
    * Registers signal handler for SIGSEGV
    */
   void register_signal_handlers() {
      fprintf(stderr, "Registering Signal Handlers\n");
      struct sigaction act;
      act.sa_sigaction = signal_handler;
      act.sa_flags = SA_SIGINFO;
      sigemptyset(&act.sa_mask);
      sigaction(SIGSEGV, &act, 0);
      return;
   }

   void nlcbsmm_init() {
      // Register the signal handlers
      register_signal_handlers();

      // Spawn the thread that listens for network commands
      int stacksize = 4096;
      void* st = mmap(NULL, stacksize , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

      //networkmanager.spawn_listener(st, stacksize);

      return;
   }
}


