#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <ctime>


#define SIZE 64

using namespace std;

#define PAGE_SIZE 4096

int main(int argc, char** argv) {
  struct timespec START;
  struct timespec END;
  long diff = 0;
  
  char * superBlock = (char *) malloc(1);
  clock_gettime(CLOCK_REALTIME, &START); 
  superBlock[1024] = 0;
  clock_gettime(CLOCK_REALTIME, &END); 

  diff = END.tv_nsec - START.tv_nsec;
  cout<<"time test1: "<< diff <<'\n';
  return 0;
}

