#include <iostream>
#include <cstdio>
#include <stdlib.h>
#include <cstdio>
#define SIZE 64
using namespace std;
int main(int argc, char** argv) {
    //cout << "Starting" << endl;
    char* test1 = (char*) malloc (sizeof(char) * 4096*5);
    
    //test1[0] = 65;
    test1[4096 + 1] = 66;
    //test1[4096*3 + 14] = 67;
      
    fprintf(stderr, "******Copy the following******\n");
    //fprintf(stderr, "i\n%p\n", test1);
    fprintf(stderr, "i\n%p\n", test1 + 4096 + 1);
    //fprintf(stderr, "i\n%p\n", test1 + 3*4096 + 14);
    fprintf(stderr, "******************************\n");
    sleep(30);
    //fprintf(stderr,"---------------------------HOARD_TEST:%c\n",test1[0]);
    fprintf(stderr,"---------------------------HOARD_TEST:%c\n",test1[4096 + 4095]);
    //fprintf(stderr,"---------------------------HOARD_TEST:%c\n",test1[4096*3 + 14]);

    fprintf(stderr,"---------------------------HOARD_TEST:NOW FREEING\n");
    free(test1);
    cout << "Exitting" << endl;
    exit(1);
}
