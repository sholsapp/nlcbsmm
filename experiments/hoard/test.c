#include <stdio.h>

char* hoardstrdup(char* s);
void hoardfree(void* p);

int main(int argc, char** argv) {
   char* thing = hoardstrdup(argc < 2 ? "world" : argv[1]);
   printf("Hello, %s!\n", thing);
   hoardfree(thing);
   return 0;
}
