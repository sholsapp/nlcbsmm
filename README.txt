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
