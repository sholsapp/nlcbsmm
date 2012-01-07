/**
 * @file huff.h
 * @author Stephen Holsapple
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define CHARSET 256
#define EXTLEN 6
#define BYTE 8

/**
 * Defines thislnode to be also called lnode
 */
typedef struct thislnode lnode;

/**
 * Defines the thislnode structure.
 *
 * Used for managing linked lists and trees.
 */
struct thislnode {
  unsigned int item;
  int count;

  /*list properties*/
  lnode* next;

  /*tree properties*/
  lnode* left;
  lnode* right;
  char isLeaf;
  
  /*encoded properties*/
  char* pattern;

}thislnode;

/**
 * @param the lnode* pointing to the root of this tree
 * @param the char* containing the current string map
 * @param the int representing the depth of the tree
 * @param the char** to the lookup table we're creating
 */
void generate(lnode* root, char* pattern, int depth, char** lookup) {
  char temp[CHARSET];
  strcpy(temp, pattern);

  if (root->left->left != NULL) {
    strcat(temp, "0");
    generate(root->left, temp, depth+1, lookup);
  }

  if (root->left->isLeaf) {
    char* newpattern = (char*)malloc(sizeof(char)*depth+1);
    strcpy(newpattern, temp);
    strcat(newpattern, "0");

    root->left->pattern = newpattern;
    lookup[(unsigned int)root->left->item] = newpattern;
  }

  if (root->right->left != NULL) {
    temp[depth-1] = '\0';
    strcat(temp, "1");
    generate(root->right, temp, depth + 1, lookup);
  }

  if (root->right->isLeaf) {
    temp[depth-1] = '\0';
    char* newpattern = (char*)malloc(sizeof(char)*depth+1);
    strcpy(newpattern, temp);
    strcat(newpattern, "1");

    root->right->pattern = newpattern;
    lookup[(unsigned int)root->right->item] = newpattern;
  }
}

/**
 * @param the char* to a pattern of BYTE many characters.
 */
int packbits(char* pattern) {
  int result = 0;
  int i = 0;
  for (i = 0; i < BYTE; i++) {
    if (pattern[i] == '1') {
      result = result | 1;
      result = result<<(i != (BYTE-1) ? 1 : 0);
    }
    else if (pattern[i] == '0') {
      result = result<<(i != (BYTE-1) ? 1 : 0);
    }
  }
  return result;
}

/**
 * @param the int to be converted to ascii 0 and 1.
 */
char* unpackbits(int bits) {
  char* result = (char*)malloc(sizeof(char)*BYTE);
  unsigned char mask = 0x80;
  int i = 0;
  for(i = 0; i < BYTE; i++) {
    result[i] = (bits&mask) == 0 ? '0' : '1';
    bits = bits<<1;
  }
  return result;
}
