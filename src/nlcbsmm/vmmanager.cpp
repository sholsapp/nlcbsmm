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

         void spawn_listener(void* stack, int size) {
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

            if((threadid = clone(&listener, child_stack, CLONE_VM | CLONE_FILES, argument)) == -1) {
               perror("vmmanager.cpp, clone");
               exit(EXIT_FAILURE);
            }
            return;
         }

         static int listener(void* t) {
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

            //do_work(sk);

            close(sk);
            return 0;
         }


      private:
         int threadid;

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

      fprintf(stderr, "        main: %p\n",  &main        );
      fprintf(stderr, "       _init: %p\n",  &_init       );
      fprintf(stderr, "       _fini: %p\n",  &_fini       );
      fprintf(stderr, "        _end: %p\n",  &_end        );
      fprintf(stderr, "__data_start: %p\n",  &__data_start);

      register_signal_handlers();

      // Spawn the thread that listens for network commands
      stack_ptr = mmap(NULL, stack_sz , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      networkmanager.spawn_listener(stack_ptr, stack_sz);
      return;
   }
}
