#pragma once

#include <psptypes.h>
#include <pspkerneltypes.h>
#include <pspsysmem.h>

typedef struct MusicNode {
    struct MusicNode* next;
    struct MusicNode* prev;
    char name[256];
    SceUID uid;
} MusicNode;

void reverse(char s[]) {
   int i, j;
   char c;

   for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
      c = s[i];
      s[i] = s[j];
      s[j] = c;
   }
}

void itoa(int n, char s[]) {
   int i, sign;
   if ((sign = n) < 0) n = -n;
   i = 0;
   do {
      s[i++] = n % 10 + '0';
   } while ((n /= 10) > 0);
   if (sign < 0) s[i++] = '-';
   s[i] = '\0';
   reverse(s);
}

static int musicAllocateCounter = 0;
MusicNode* createMusicNode(char * name) {
   char buf[10];
   char mname[16];
   strcpy(mname, "glmn");
   itoa(musicAllocateCounter, buf);
   strcat(mname, buf);
   SceUID uid = sceKernelAllocPartitionMemory(2, mname, PSP_SMEM_High, sizeof(MusicNode), NULL);
   if (uid < 0) {
      return NULL;
   }
   MusicNode* newNode = (MusicNode*)sceKernelGetBlockHeadAddr(uid);
   if (newNode != NULL) {
      newNode->next = NULL;
      newNode->prev = NULL;
      strncpy(newNode->name, name, sizeof(newNode->name) - 1);
      newNode->name[sizeof(newNode->name) - 1] = 0;
      newNode->uid = uid;
   } else {
      sceKernelFreePartitionMemory(uid);
      return NULL;
   }
   musicAllocateCounter++;
   return newNode;
}

void appendMusicNode(MusicNode** head, MusicNode* newNode) {
   if (*head == NULL) {
      *head = newNode;
   } else {
      MusicNode* current = *head;
      while (current->next != NULL) {
         current = current->next;
      }
      current->next = newNode;
      newNode->prev = current;
   }
}

void freeMusicList(MusicNode* head) {
   if (head == NULL) {
      return;
   }
   MusicNode* current = head;
   while (current != NULL) {
      MusicNode* nextNode = current->next;
      sceKernelFreePartitionMemory(current->uid);
      current = nextNode;
   }
}