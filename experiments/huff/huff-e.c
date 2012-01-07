/**
 * @file huff-e.c
 *
 * This program generates a binary file from an ASCII text file.
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
main (int argc, char* argv[]) {
  FILE* input = stdin;
  char** lookup = (char**)malloc(sizeof(char*)*CHARSET);
  unsigned int in;

  const char* temppath = "huff-e.orig.temp";
  FILE* tempfile = fopen(temppath, "w+");
  unlink(temppath);
  if (tempfile == NULL) {
    fprintf(stderr, "Error creating %s.\n", temppath);
    exit(EXIT_FAILURE);
  }

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

    int arr[CHARSET] = {0};
    int count = 0;

    while (((in = fgetc(input))) != EOF) {
      if (fputc(in, tempfile) == EOF) {
        fprintf(stderr, "Error writing temp file.\n");
        exit(EXIT_FAILURE);
      }
      arr[in] = arr[in] + 1;
      count++;
    }

    int itemcount = count-1;

    if(fseek(tempfile, 0, SEEK_SET)) {
      fprintf(stderr, "Error seeking on %s.\n", temppath);
    }

    int i = 0;
    int maxi = 0;
    int max = 0;
    while (count > 0) {
      for (i = 0; i < CHARSET; i++) {
        if (arr[i] > max && arr[i] != 0) {
          maxi = i;
          max = arr[i];
        }
      }

      count = count - max;
      if (head == NULL) {
        head = (lnode*)malloc(sizeof(lnode));
        head->item = maxi;
        head->count = max;
        head->isLeaf = TRUE;
        head->next = NULL;
      }
      else {
        current = (lnode*)malloc(sizeof(lnode));
        current->item = maxi;
        current->count = max;
        current->isLeaf = TRUE;
        temp = head;
        staticlist = head = current;
        current->next = temp;
      }
      arr[maxi] = 0;
      max = 0;
    }

    //build a binary tree
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
    
    generate(head, "", 1, lookup);

    int pathlen = strlen(argv[done]);
    char* outname = (char*)malloc(sizeof(char)*pathlen + EXTLEN);
    strcpy(outname, argv[done]);
    strcat(outname, ".huff");
    FILE* output = fopen(outname, "w");

    fprintf(output, "%d ", itemcount);
    current = staticlist;
    while (current != NULL) {
      if (current->isLeaf) {
        fprintf(output, "%d %d ", current->item, current->count);
      }
      current = current->next;
    }
    //fprintf(output, "\n");

    char* asciioutpath = "huff-e.ascii.temp";
    FILE* asciiout = fopen(asciioutpath, "w+");
    unlink(asciioutpath);
    if (asciiout == NULL) {
      fprintf(stderr, "Error creating %s.\n", asciioutpath);
      exit(EXIT_FAILURE);
    }
   
    while (((in = fgetc(tempfile))) != EOF) {
      fputs(lookup[in], asciiout);
    }

    if(fseek(asciiout, 0, SEEK_SET)) {
      fprintf(stderr, "Error seeking on %s.\n", asciioutpath);
    }
  
    char* pattern = (char*)malloc(sizeof(char)*BYTE);
    unsigned int result = 0;
    count = 0;

    while ((in = fgetc(asciiout)) != EOF || count != 0) {
      if (feof(asciiout)) {
        while (count != BYTE) {
          pattern[count] = '0';
          count++;
        }
        result = packbits(pattern);
        fprintf(output, "%c", result);
        break;
      }
      if (in == '1' || in == '0') {
        pattern[count] = in;
        count++;
      }
      if (count == BYTE) {
        result = packbits(pattern);
        fprintf(output, "%c", result);
        count = 0;
      }
    }
    fclose(output);
    fclose(input);
  }
}
