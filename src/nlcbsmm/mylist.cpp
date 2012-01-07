#include <cstdio>
#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include "vmmanager.h"

//#include "hoard.h"

//using namespace Hoard;

namespace NLCBSMM {

   SBEntry* LinkedList::findSuperblock(void* ptr) {
      //SmallSuperblockType* t;
      TreeNode* temp = head;
      while (temp != NULL) {

         //t = reinterpret_cast<SmallSuperblockType*>(temp->data->sb);

         // Check if a valid pointer (valid starts at the end of the superblock's header)

         unsigned long p = (unsigned long) ptr;
         unsigned long sb = (unsigned long) temp->data->sb;
         unsigned long end = sb + (4096 * 16);

         if (p >= sb && p < end) {
            //if (t->inRange((void*)((int)ptr + 72))) {
            // Found it
            return temp->data;
         }
         else {
            temp = temp->right;
         }
         }
         // We aparently didn't find it
         return NULL;
      }

      void LinkedList::insert(SBEntry* data) {
         if (head == NULL) {
            //head = source->allocNode();
            head = (TreeNode*) heap->malloc(sizeof(TreeNode));
            head->data = data;
            head->right = NULL;
            head->left = NULL;
            tail = head;
         }
         else {
            //tail->right = source->allocNode();
            tail->right = (TreeNode*) heap->malloc(sizeof(TreeNode));
            tail->right->data = data;
            tail->right->right = NULL;
            tail->right->left = NULL;
            tail = tail->right;
         }
         sz++;
         return;
      }

      int LinkedList::getSize() {
         return sz;
      }

      SBEntry* LinkedList::get(int index) {
         assert(sz > 0);
         TreeNode* ret = NULL;
         int curIndex = 0;

         ret = head;
         while(curIndex != index){
            ret = ret->right;
            curIndex++;
         }
         return ret->data;
      }




   }
