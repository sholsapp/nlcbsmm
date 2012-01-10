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

***************** TODO *******************

1) Testbed clients.

  a) Handshake to get addr space information from master.
  
  b) Identify .text addr space info from the master.
  
    i) How can we guarantee that the .text from the master (the one that the program-thread needs to execute over) doesn't collide with the nlcbsmm-client's .text address space?
    
    ia) In the client process, heap-allocate a process worth of memory (4GB+?) and start the pthread_create() code there.  Let it fault/map pages into this heap-allocated memory region, leaving nlcbsmm-client addr space intact.
    
  c) Ability to list pages available on the server.
  
  d) Ability to fault in pages available on the server (manually).
  
  e) Ability to tell server that it wants to fault back a page the nlcbsmm-client currently has.
 
2) Intercepting pthread_create().

3) Virtual memory manager.

4) Fix janky list abstraction.

