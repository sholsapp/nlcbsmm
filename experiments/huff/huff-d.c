/**
 * @file huff-d.c
 *
 * This program generates an ASCII text file from a binary file.
 *
 * @author Stephen Holsapple
 */

#include "huff.h"

/**
 * The main thing.
 * @param the int argc the number of command line arguments.
 * @param the char* array holding command line tokens.
 */
int
main(int argc, char* argv[]) {
  FILE* input;
  unsigned int in;
  char* bits;
    
  lnode* staticlist = (lnode*)malloc(sizeof(lnode));
  lnode* head = NULL;
  lnode* current = NULL;
  lnode* temp = NULL;
  lnode* working = NULL;
  lnode* prev = NULL;
  
  int done = 1;
  for (done = 1; done < argc; done++) {
    input = fopen(argv[done], "r");
    if (input == NULL) {
      fprintf(stderr, "Error opening file %d\n", done);
      exit(EXIT_FAILURE);
    }

    unsigned int ascii = 0;
    int freq = 0;
    int itemcount = 0;
    fscanf(input, "%d ", &itemcount);
    while (fscanf(input, "%u %d", &ascii, &freq) != 0) {
      if (head == NULL) {
        head = (lnode*)malloc(sizeof(lnode));
        head->item = ascii;
        head->count = freq;
        head->isLeaf = TRUE;
        current = staticlist = head;
      }
      else {
        current->next = (lnode*)malloc(sizeof(lnode));
        current->next->item = ascii;
        current->next->count = freq;
        current->next->isLeaf = TRUE;
        current = current->next;
      }
 
    }
   
    while (head->next != NULL) {
      temp = head;
      head = head->next;
      working = (lnode*)malloc(sizeof(lnode));
      working->count = temp->count + head->count;
      working->left = temp;
      working->right = head;
      working->isLeaf = FALSE;
      current = head->next;
      prev = NULL;
      while (current != NULL && current->count <= working->count) {
        prev = current;
        current = current->next;
      } 
      if (prev == NULL) {
        working->next = current;
        head = working;
      }
      else {
        prev->next = working;
        working->next = current;
        head = head->next;
      }
    }

    int pathlen = strlen(argv[done]);
    char* outname = (char*)malloc(sizeof(char)*pathlen+1);
    strncpy(outname, argv[done], pathlen-5);
    //strcat(outname, ".deco");
    FILE* output = fopen(outname, "w");
   

    char* asciioutpath = "huff-d.ascii.temp";
    FILE* asciiout = fopen(asciioutpath, "w+");
    unlink(asciioutpath);
    if (asciiout == NULL) {
      fprintf(stderr, "Error creating %s.\n", asciioutpath);
      exit(EXIT_FAILURE);
    }

    int count = 0;
    while ((in = fgetc(input)) != EOF) {
      bits = unpackbits(in);
      fprintf(asciiout, "%s", bits);
      count++;
    }

    if(fseek(asciiout, 0, SEEK_SET)) {
      fprintf(stderr, "Error seeking on %s.\n", asciioutpath);
      exit(EXIT_FAILURE);
    }

    current = head;
    while ((in = fgetc(asciiout)) != EOF || itemcount > 0) {
      current = in == '1' ? current->right : current->left;
      if (current->isLeaf) {
        fputc(current->item, output);
        current = head;
        itemcount--;
      }
    }
    //fputc('\n', output);
    fclose(input);
    fclose(output);
  }
}
