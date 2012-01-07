#include <sys/socket.h>
#include <netinet/in.h>

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
         }

         virtual int getSize() {
            return sizeof(NetworkLocation);
         }

         virtual void* getPage() {
            // Connect to the remote host
            // Send GETPAGE request
            // Read GETPAGE_RESPONSE response

         }

      private:
         struct sockaddr_in host;

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

            fprintf(stderr, "Zeroing page at %p\n", page);

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
