                            _.--....
                 _....---;:'::' ^__/
               .' `'`___....---=-'`
              /::' (`
              \'   `:.
               `\::.  ';-"":::-._  {}
            _.--'`\:' .'`-.`'`.' `{I}
         .-' `' .;;`\::.   '. _: {-I}`\
       .'  .:.  `:: _):::  _;' `{=I}.:|
      /.  ::::`":::` ':'.-'`':. {_I}::/
      |:. ':'  :::::  .':'`:. `'|':|:'
       \:   .:. ''' .:| .:, _:./':.|
       '--.:::...---'\:'.:`':`':./
                       '-::..:::-' nlcbsmm


******************************************
***************** ABOUT ******************
******************************************
Northern Lights Cinerious Brown Snake Memory Manager (NLCBSMM) is a distributed shared memory system implemented in user-space.  By extending
the Hoard virtual memory manager to resolve faults via networked machines and support for thread/process migration using a modified pthread
interface, NLCBSMM enables applications to link with a modified libhoard.so to run on the distributed system with no modifications to the
original application.

Boom.

Linux support only.


******************************************
*********** COMPILATION NOTES ************
******************************************

For compiling applications with Hoard, you may need to use the '-Wl,--no-as-needed' and '-Wl,-R,/path/to/libhoard/dir':
 (thanks to cyk on irc.freenode.net #stackoverflow for this http://stackoverflow.com/questions/8814707/shared-library-mysteriously-doesnt-get-linked-to-application)

> g++ -O2 -Wall -o hoard_test hoard_test.cpp -Wl,--no-as-needed -Wl,-R,/home/sholsapp/workspace/nlcbsmm/experiments/hoard -L. -lhoard


******************************************
***************** TODO *******************
******************************************

1) Testbed clients.

  a) Handshake to get addr space information from master.
  
  b) Identify .text addr space info from the master.
  
  c) Ability to list pages available on the server.
  
  d) Ability to fault in pages available on the server (manually).
  
  e) Ability to tell server that it wants to fault back a page the nlcbsmm-client currently has.
 
2) Intercepting pthread_create().

3) Virtual memory manager.


******************************************
************* SCRATCH PAD ****************
******************************************

#inclue <queue>
  
 class Machine {
   public:
     // Ip Address
     // TODO: encode page table in here?
 };
  
 class NetworkServer {
   public:
     stl::queue<Machine> cluster;
    
      NetworkServer() {
        // Multicast on port X
        // Select 5 seconds
        if (data) {
          // You're not the primary
          // Wait for work
          
        }
        else {
          // You're the primary
          // Listen
        }

      } 
     
 };
  
 class HelloPacket {
   /**
    * Multicasted to everyone by a new machine.
    */
   public:
     uint8_t flag;
 }__attribute__((packed)); 


class MachineDescPacket {
 /**
  * A description of a network machine.
  */
  public:
    // Which machine this packet describes
    uint32_t seq;
    uint32_t ip;
    uint32_t length;
}__attribute__((packed));


 class HelloResponsePacket {
   /**
    * Sent from the master node to the new machine.
    */
   public:
     uint8_t flag;
     // This is how many packets we're about to send you
     uint32_t num_machines;
 }__attribute__((packed));
  
  
 class HandshakePacket {
   public:
     uint32_t start_text;
     uint32_t end_text;
     
     uint16_t cksum;
     uint32_t length;
     
 }__attribute__((packed));


