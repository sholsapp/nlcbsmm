#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "packets.h"


int setup_socket(char* host_name, struct sockaddr_in* remote);

int main(int argc, char * argv[]) {
   int sk         = 0; 
   int send_len   = 0;
   int sent       = 0;
   int read       = 0;
   char send_type = 0;
   int bufsize    = 4096 + 8;
   char send_buf[4096 + 8];
   char read_buf[4096 + 8];
   struct sockaddr_in remote;

   if(argc!= 2) {
      printf("Usage: %s host-name(vogon,unix1,etc) \n", argv[0]);
      exit(1);
   }

   /* set up the socket for TCP transmission  */
   sk= setup_socket(argv[1],  &remote);

   while(send_type != 'e') {

      memset(send_buf, 0, bufsize);

      printf("input >> ");
      send_type = getchar();

      // Contrived test (shadow page)
      if(send_type == 'i') {
         send_len = sizeof(int);//1 char and a long
         printf("Enter page addr to invalidate: ");

         read = scanf("%s",send_buf);

         // Cleans up garbage in buffer
         do {/*nothing*/ } while(getchar() != '\n');

         if(read){
            send_buf[10] = 0;
            unsigned int page = strtoul(send_buf, NULL, 16);
            printf("Sending: invalidate command for page %x\n", page);

            struct NLCBSMMpacket* packet = (struct NLCBSMMpacket*)send_buf;
            packet->type = 0x0;
            packet->page = (unsigned int)htonl(page);

            // now send the data
            sent =  sendto(sk, send_buf, sizeof(int)+sizeof(char), 0,(struct sockaddr*) &remote, sizeof(struct sockaddr));
            if(sent < 0) {
               perror("send call");
               exit(EXIT_FAILURE);
            }
         }
      }

      // Contrived test (COR)
      else if (send_type == 'c') {

         // Shenanigans
         struct NLCBSMMpacket_init* packet = (struct NLCBSMMpacket_init*) send_buf;
         packet->type = 0x2;
         // Send an init command
         if (sendto(sk, send_buf, sizeof(struct NLCBSMMpacket_init), 0, (struct sockaddr*) &remote, sizeof(struct sockaddr)) < 0) {
            perror("send call (c)");
            exit(EXIT_FAILURE);
         }
         // Clear send buffer, just in case
         memset(send_buf, 0, bufsize);

         fprintf(stderr, ">> Client sent INIT packet\n");

         // Shenanigans
         socklen_t fromLen = sizeof(remote);

         // Read response (a int X followed by X many ints)
         memset(read_buf, 0, bufsize);
         read = recvfrom(sk, read_buf, bufsize, 0, (struct sockaddr*) &remote, &fromLen);

         fprintf(stderr, ">> Client read response\n");

         // Take the first one, calculate a page in the middle of that superblock (use page 1)
         struct NLCBSMMpacket_init_response* response_packet = (struct NLCBSMMpacket_init_response*) read_buf;

         unsigned int addr = response_packet->sb_addrs[0];

         fprintf(stderr, ">> Client pulled out: %p\n", (void*) addr);

         // Lie to the server, say you own page 1
         struct NLCBSMMpacket* lie = (struct NLCBSMMpacket*) send_buf;
         // Set the correct type of packet (see server code)
         lie->type = 0x4;
         // Say we own the second page in the superblock
         lie->page = addr + 4096;
         if (sendto(sk, send_buf, sizeof(struct NLCBSMMpacket), 0, (struct sockaddr*) &remote, sizeof(struct sockaddr)) < 0) {
            perror("send call (c)");
            exit(EXIT_FAILURE);
         }
         memset(send_buf, 0, bufsize);

         fprintf(stderr, ">> Lied to server (faker owns %p)\n", (void*) (addr + 4096));


         // Wait for request from NetworkLocation client
         memset(read_buf, 0, bufsize);
         read = recvfrom(sk, read_buf, bufsize, 0, (struct sockaddr*) &remote, &fromLen);

         fprintf(stderr, ">> Client read response\n");

         // Find out what page they want
         // Second page of the superblock

         // Send a fake page with some good data in it
         char* page = (char*) malloc(4096);
         for (int i = 0; i < 4096; i++) {
            // Should be the '@' character
            page[i] = 64;
         }
         fprintf(stderr, ">> Prepared fake page, first byte = %c\n", page[0]);

         // Send page to server
         struct NLCBSMMpacket_sendpage* page_response = (struct NLCBSMMpacket_sendpage*) send_buf;

         page_response->type = 0x7;
         page_response->addr = addr + 4096;
         memcpy(page_response->page, page, 4096);

         if (sendto(sk, send_buf, sizeof(struct NLCBSMMpacket_sendpage), 0, (struct sockaddr*) &remote, sizeof(struct sockaddr)) < 0) {
            perror("send call (c)");
            exit(EXIT_FAILURE);
         }
         memset(send_buf, 0, bufsize);

         fprintf(stderr, "Sent off page (%p)\n", (void*) (addr + 4096));

      }

      // Kill the server
      else if(send_type == 'e') {
         // Shenanigans
         struct NLCBSMMpacket* packet = (struct NLCBSMMpacket*)send_buf;
         // Ending 1 byte
         packet->type = 0x1;//magic number for kill command
         // Now send the data
         sent =  sendto(sk, send_buf, sizeof(char), 0,(struct sockaddr*) &remote, sizeof(struct sockaddr));
         if(sent < 0) {
            perror("send call");
            exit(EXIT_FAILURE);
         }
         fprintf(stderr,"Sent kill command\n");

      }
   }

   close(sk);
   return 0;
}


int setup_socket(char *host_name, struct sockaddr_in* remote) {
   /**
    *
    */
   int            sk  = 0;
   struct hostent *hp = NULL;
   if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("socket call");
      exit(EXIT_FAILURE);
   }
   if ((hp = gethostbyname(host_name)) == NULL) {
      printf("Error getting hostname: %s\n", host_name);
      exit(EXIT_FAILURE);
   }
   memcpy((char*)&remote->sin_addr, (char*)hp->h_addr, hp->h_length);
   remote->sin_family= AF_INET;
   remote->sin_port= htons(6666);
   return sk;
}

