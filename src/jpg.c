#pragma once

#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <ctype.h>

#include "include/picojpeg.c"
#include "include/simpledecoder.c"

#define OUTPUT_SIZE 64
#define OUTPUT_BPP 4

#include "include/logo.h"

int syncsafe_to_int_psp(unsigned char bytes[4]) {
   return (bytes[0] << 21) | (bytes[1] << 14) | (bytes[2] << 7) | bytes[3];
}

int be_to_int_psp(unsigned char bytes[4]) {
   return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

int strcicmp(char const *a, char const *b) {
   for (;; a++, b++) {
      int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
      if (d != 0 || !*a) return d;
   }
}

void resize_image_64x64(unsigned char *dst_data, const unsigned char *src_data, int src_width, int src_height, int src_comps) {
   float x_ratio = (float)src_width / OUTPUT_SIZE;
   float y_ratio = (float)src_height / OUTPUT_SIZE;

   for (int y = 0; y < OUTPUT_SIZE; y++) {
      for (int x = 0; x < OUTPUT_SIZE; x++) {
         int src_x = (int)(x * x_ratio);
         int src_y = (int)(y * y_ratio);
         int dst_index = (y * OUTPUT_SIZE + x) * OUTPUT_BPP;
         int src_index = (src_y * src_width + src_x) * src_comps;

         if (src_comps == 1) {
            dst_data[dst_index + 0] = src_data[src_index];
            dst_data[dst_index + 1] = src_data[src_index];
            dst_data[dst_index + 2] = src_data[src_index];
         } else {
            dst_data[dst_index + 0] = src_data[src_index + 0];
            dst_data[dst_index + 1] = src_data[src_index + 1];
            dst_data[dst_index + 2] = src_data[src_index + 2];
         }
         dst_data[dst_index + 3] = 255;
      }
   }
}

SceUID readPictureFromFileMP3(SceUID fd) {
   const SceSize outputSize = OUTPUT_SIZE * OUTPUT_SIZE * OUTPUT_BPP;

   unsigned char header[10];
   if (sceIoRead(fd, header, 10) != 10 || memcmp(header, "ID3", 3) != 0) {
      return -1;
   }

   int tag_size = syncsafe_to_int_psp(&header[6]);
   long tag_end = 10 + tag_size;

   SceUID resultUid = 0;

   while (sceIoLseek(fd, 0, PSP_SEEK_CUR) < tag_end) {
      unsigned char frame_header[10];
      if (sceIoRead(fd, frame_header, 10) != 10) break;
      if (frame_header[0] == 0) break;

      char frame_id[5] = {0};
      memcpy(frame_id, frame_header, 4);
      int frame_size = be_to_int_psp(&frame_header[4]);

      if (memcmp(frame_id, "APIC", 4) == 0) {
         SceUID dataUid = sceKernelAllocPartitionMemory(2, "apic_data", PSP_SMEM_High, frame_size, NULL);
         if (dataUid < 0) {
            return -1;
         }
         unsigned char *data = (unsigned char *)sceKernelGetBlockHeadAddr(dataUid);
         sceIoRead(fd, data, frame_size);

         int pos = 0;
         pos++;
         char *mime = (char *)&data[pos];
         int mime_len = strlen(mime);
         pos += mime_len + 1;
         pos++;
         char *desc = (char *)&data[pos];
         int desc_len = strlen(desc);
         pos += desc_len + 1;
         int image_data_size = frame_size - pos;

         char *fC = strrchr(mime, '/');
         if (fC != NULL) *fC = '.';

         SceBool isJpg = 0;
         if (fC != NULL) {
            isJpg = strcicmp(fC, ".jpg") || strcicmp(fC, ".jpeg");
         }

         if (!isJpg) {
            sceKernelFreePartitionMemory(dataUid);
            return -1;
         }

         int w = 0, h = 0, comps = 0;
         SceUID decodedDataUid = pjpeg_decode_from_memory(&data[pos], image_data_size, &w, &h, &comps);

         if (decodedDataUid == 0 || w <= 0 || h <= 0) {
            if (decodedDataUid) sceKernelFreePartitionMemory(decodedDataUid);
            sceKernelFreePartitionMemory(dataUid);
            return -1;
         }

         void * decodedData = sceKernelGetBlockHeadAddr(decodedDataUid);

         resultUid = sceKernelAllocPartitionMemory(2, "mp3_thumb", PSP_SMEM_High, outputSize, NULL);
         if (resultUid >= 0) {
            unsigned char *outputData = (unsigned char *)sceKernelGetBlockHeadAddr(resultUid);
            resize_image_64x64(outputData, decodedData, w, h, comps);
         }

         sceKernelFreePartitionMemory(decodedDataUid);
         sceKernelFreePartitionMemory(dataUid);
         return resultUid;
      } else {
         sceIoLseek(fd, frame_size, PSP_SEEK_CUR);
      }
   }

   return resultUid;
}
