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


*******************************************************************************
******************************** ABOUT ****************************************
*******************************************************************************
Northern Lights Cinerious Brown Snake Memory Manager (NLCBSMM) is a distributed 
shared memory system implemented in user-space.  By extending the Hoard virtual 
memory manager to resolve faults via networked machines and support for thread
or process migration using a modified pthread interface, NLCBSMM enables appli-
-cations to link with a modified libhoard.so to run on the distributed system 
with no modifications to the original application.

Boom.

Linux support only.


*******************************************************************************
*************************** COMPILATION NOTES *********************************
*******************************************************************************

For compiling applications with Hoard, you may need to use the '-Wl,--no-as-needed' and '-Wl,-R,/path/to/libhoard/dir':
 (thanks to cyk on irc.freenode.net #stackoverflow for this http://stackoverflow.com/questions/8814707/shared-library-mysteriously-doesnt-get-linked-to-application)

> g++ -O2 -Wall -o hoard_test hoard_test.cpp -Wl,--no-as-needed -Wl,-R,/home/sholsapp/workspace/nlcbsmm/experiments/hoard -L. -lhoard


*******************************************************************************
********************************** TODO ***************************************
*******************************************************************************

1) Testbed clients.

  a) Handshake to get addr space information from master.
  
  b) Identify .text addr space info from the master.
  
  c) Ability to list pages available on the server.
  
  d) Ability to fault in pages available on the server (manually).
  
  e) Ability to tell server that it wants to fault back a page the nlcbsmm-client currently has.
 
2) Intercepting pthread_create().

3) Virtual memory manager.
