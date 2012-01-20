#include <cstdio>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>

// Order matters
#include "vmmanager.h"
#include "hoard.h"
#include "packets.h"

#include <vector>

#define PAGE_SIZE 4096

/**
 * Symbols defined by the end application.
 *
 * If we use these to sanity check, cannot compile with -Wl,--no-undefined
 * because these symbols won't be defined until application link time...
 */
extern unsigned char* main;
extern unsigned char* _init;
extern unsigned char* _fini;
extern unsigned char* _end;
extern unsigned char* __data_start;

namespace NLCBSMM {
   /**
    * If we need memory to use for hoard, this is where we get it.
    */
   FreelistHeap<MmapHeap> myheap;

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
}

#define HELLO_PORT 12345
#define HELLO_GROUP "225.0.0.37"
#define MSGBUFSIZE 256

//using namespace Hoard;
//using namespace HL;

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
            speaker_thread_id  = 0;
            listener_thread_id = 0;
         }

         ~NetworkManager(){
            /**
             * Destructor
             */
            int pid = 0;

            if((pid = waitpid(speaker_thread_id, NULL,  __WCLONE)) == -1) {
               fprintf(stderr, "Return from wait (pid = %d)\n", pid);
               perror("wait error");
            }

            if((pid = waitpid(listener_thread_id, NULL,  __WCLONE)) == -1) {
               fprintf(stderr, "Return from wait (pid = %d)\n", pid);
               perror("wait error");
            }
         }

         void start_comms() {
            /**
             * Spawns the speaker and listener threads.
             */
            void* argument       = NULL;
            void* speaker_ptr    = NULL;
            void* speaker_stack  = NULL;
            void* listener_ptr   = NULL;
            void* listener_stack = NULL;
            int   size           = 4096;

            listener_stack = (void*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            speaker_stack  = (void*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            argument       = (void*) 5;
            speaker_ptr    = (void*) (((int) speaker_stack) + size);
            listener_ptr   = (void*) (((int) listener_stack) + size);

            if((speaker_thread_id = clone(&speaker, speaker_ptr, CLONE_VM | CLONE_FILES, argument)) == -1) {
               perror("vmmanager.cpp, clone, speaker");
               exit(EXIT_FAILURE);
            }

            if((listener_thread_id = clone(&listener, listener_ptr, CLONE_VM | CLONE_FILES, argument)) == -1) {
               perror("vmmanager.cpp, clone, listener");
               exit(EXIT_FAILURE);
            }

            return;
         }

         static int speaker(void* t) {
            /**
             *
             */
            fprintf(stderr, "> speaker\n");

            uint32_t sk             = 0;
            uint32_t cnt            = 0;
            struct ip_mreq mreq     = {0};
            struct sockaddr_in addr = {0};

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("vmmanager.cpp, socket");
               exit(EXIT_FAILURE);
            }

            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = inet_addr(HELLO_GROUP);
            addr.sin_port        = htons(HELLO_PORT);

            char message[] = "Hello, World!";

            while (1) {
               if (sendto(sk, message, sizeof(message), 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                  perror("vmmanager.cpp, sendto");
                  exit(EXIT_FAILURE);
               }
               sleep(EXIT_FAILURE);
            }

            return 0;

         }


         static int listener(void* t) {
            /**
             *
             */
            fprintf(stderr, "> listener\n");

            uint32_t sk             =  0;
            uint32_t nbytes         =  0;
            uint32_t addrlen        =  0;
            uint32_t yes            =  1;
            struct ip_mreq mreq     = {0};
            struct sockaddr_in addr = {0};

            char msgbuf[MSGBUFSIZE];

            if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
               perror("vmmanager.cpp, socket");
               exit(EXIT_FAILURE);
            }

            // Allow multiple sockets to use the same port
            if (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
               perror("vmmanager.cpp, setsockopt");
               exit(EXIT_FAILURE);
            }

            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port        = htons(HELLO_PORT);

            if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
               perror("vmmanager.cpp, bind");
               exit(EXIT_FAILURE);
            }

            // Use setsockopt() to join multicast group
            mreq.imr_multiaddr.s_addr = inet_addr(HELLO_GROUP);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            if (setsockopt(sk, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
               perror("vmmanager.cpp, setsockopt");
               exit(EXIT_FAILURE);
            }

            char ip[1024];
            inet_ntop(AF_INET, &addr.sin_addr.s_addr, ip, sizeof(ip));

            while (1) {
               addrlen=sizeof(addr);
               if ((nbytes=recvfrom(sk,msgbuf,MSGBUFSIZE,0,
                           (struct sockaddr *) &addr, &addrlen)) < 0) {
                  perror("recvfrom");
                  exit(1);
               }
               fprintf(stderr, "%s: %s\n", ip, msgbuf);
            }
            return 0;
         }

      private:
         int speaker_thread_id;
         int listener_thread_id;

   };


   /**
    * This is the distributed system server
    */
   NetworkManager networkmanager;


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
         perror("vmmanager.cpp: mprotect");
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

      fprintf(stderr, "        main: %p\n",  &main        );
      fprintf(stderr, "       _init: %p\n",  &_init       );
      fprintf(stderr, "       _fini: %p\n",  &_fini       );
      fprintf(stderr, "        _end: %p\n",  &_end        );
      fprintf(stderr, "__data_start: %p\n",  &__data_start);

      register_signal_handlers();

      // Spawn the thread that speaks/listens to cluster
      networkmanager.start_comms();
      return;
   }
}
