#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include "packets.h"

namespace NLCBSMM {
   // Super class to represent a location of a page
   class Location {
      public:
         virtual int getSize() = 0;
         virtual void* getPage() = 0;
   };

   // Class to represent a page that resides in a chunk of memory
   class MemoryLocation : public Location {
      public:
         MemoryLocation(void* ptr) {
            actualPage = ptr;
            sz = 4096;
         }

         virtual int getSize() {
            return sizeof(MemoryLocation);
         }

         virtual void* getPage() {
            return actualPage;
         }

      private:
         void* actualPage;
         int sz;
   };

   class NetworkLocation : public Location {
      public:

         NetworkLocation(struct sockaddr_in remote) {
            host = remote;
            fromlen = sizeof(host);
            guard = false;
            bufsize = 4096 + 8;
            actualPage = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

            socket_num  = socket(AF_INET,SOCK_DGRAM,0);
            //fprintf(stderr,"SOCKET IS %d\n", socket_num);

            struct sockaddr_in local;
            socklen_t len = sizeof(local);
            //make sure it is empty
            memset(&local, 0, sizeof(struct sockaddr_in));
            local.sin_family = AF_INET;
            local.sin_addr.s_addr = INADDR_ANY;
            local.sin_port = 0;//random port

            /* bind the name (address) to a port */
            if (bind(socket_num, (struct sockaddr *) &local, sizeof(local)) < 0) {
               perror("bind call");
               exit(-1);
            }

            //get the port name and print it out
            if (getsockname(socket_num, (struct sockaddr*)&local, &len) < 0) {
               perror("getsockname call");
               exit(-1);
            }
            printf("NetworkLocation socket has port %d \n", ntohs(local.sin_port));
         }

         ~NetworkLocation() {
            munmap(actualPage, PAGESIZE);
            close(socket_num);
         }

         virtual int getSize() {
            return sizeof(NetworkLocation);
         }

         virtual void* getPage() {

            //fprintf(stderr, "^^ NetworkLocation::getPage()\n");

            // We don't have the page yet
            if (!guard) {
               (void) fetchNetworkPage();
               // Can do post-processing on the page here
            }
            return actualPage;
         }



      private:

         void fetchNetworkPage() {
            /*
            // send buffer
            unsigned char sendbuf[4096 + 8];
            // read buffer
            unsigned char recbuf[4096 + 8];


            // Clear buffer for safety
            memset(sendbuf, 0, bufsize);

            // Cast buffer as packet
            GetPagePacket* mypack = (GetPagePacket*) sendbuf;
            mypack->type = NLCBSMM_GET_PAGE;

            // Send packet, exit on failure
            if (sendto(socket_num, sendbuf, sizeof(GetPagePacket), 0, (struct sockaddr*) &host, sizeof(struct sockaddr)) < 0) {
               perror("NetWorkLocation::getPage()");
               exit(-1);
            }

            // Clear buffer for safety
            memset(recbuf, 0, bufsize);
            int read = recvfrom(socket_num, recbuf, bufsize, 0, (struct sockaddr*) &host, &fromlen);
            //fprintf(stderr, ">> Received data (%d)!\n", read);

            SendPagePacket* thrpack = (SendPagePacket*) recbuf;

            // We may be retarded check
            assert(thrpack->type == NLCBSMM_SEND_PAGE);

            //fprintf(stderr, ">> Page (%p)\n", (void*) thrpack->addr);
            //fprintf(stderr, ">> Page[0] = %c\n", (char) thrpack->page[0]);

            memcpy(actualPage, thrpack->page, 4096);

            guard = true;

            return;
            */
         }

         bool guard;
         void* actualPage;
         int socket_num;
         socklen_t fromlen;
         struct sockaddr_in host;
         int bufsize;
   };

   class ShadowMemoryLocation : public Location {
      public:
         ShadowMemoryLocation() {
            actualPage = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            sz = 4096;
         }

         ~ShadowMemoryLocation() {
            munmap(actualPage, PAGESIZE);
         }

         virtual int getSize() {
            return PAGESIZE;
         }

         virtual void* getPage() {
            //return NULL;
            return actualPage;
         }

         /**
          * Takes a pointer to the "real" page and copies
          *  the contents of actualPage to it.
          */
         void setPage(void* page) {
            //move the page
            memcpy(actualPage, page, PAGESIZE);

            //fprintf(stderr, "Zeroing page at %p\n", page);

            memset(page, 0, 4096);
            return;
         }

      private:
         void* actualPage;
         int sz;
   };


   // Class to represent a page that resides in a file
   class FileLocation : public Location {
      public:
         virtual int getSize() {
            return sizeof(FileLocation);
         }
         virtual void* getPage() {
            return NULL;
         }
   };

}
