#include <malloc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

// 64kB stack
#define FIBER_STACK 1024*64
#define GetSP(sp)  asm("movl  %%esp,%0": "=r" (sp) : )


using namespace std;

void signal_handler(int signo, siginfo_t* info, void* contex) {
   printf("Signal Handler\n");
   cout << "Tried accessing " << info->si_addr << endl;
   exit(-1);
}

int recur(int v, unsigned long osp) {
  unsigned  long sp = 0;
  GetSP(sp);
  //cout << sp << " ";
  fprintf(stderr, "%p\n", (void*) (sp - osp));
  //cout << v << " ";
  recur(v + 1, osp);
}

// The child thread will execute this function
int threadFunction( void* argument )
{
   //printf( "child thread exiting\n" );
   //*((int *) 0) = 1;
   unsigned long sp = 0;
   GetSP(sp);
   recur(1, sp);
   cout << "Should Never Get Here" << endl;
   return 0;
}

int main()
{
   void* stack;
   pid_t pid;

   struct sigaction act;
   act.sa_sigaction = signal_handler;
   act.sa_flags = SA_SIGINFO;
   sigemptyset(&act.sa_mask);
   sigaction(SIGSEGV, &act, 0);

   // Allocate the stack
   stack = malloc( FIBER_STACK );
   if ( stack == 0 )
   {
      perror( "malloc: could not allocate stack" );
      exit( 1 );
   }

   printf( "Creating child thread\n" );

   // Call the clone system call to create the child thread
   pid = clone( &threadFunction, (char*) stack + FIBER_STACK,
         SIGCHLD | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM, 0 );
   if ( pid == -1 )
   {
      perror( "clone" );
      exit( 2 );
   }

   // Wait for the child thread to exit
   pid = waitpid( pid, 0, 0 );
   if ( pid == -1 )
   {
      perror( "waitpid" );
      exit( 3 );
   }

   // Free the stack
   free( stack );

   printf( "Child thread returned and stack freed.\n" );

   return 0;
}
