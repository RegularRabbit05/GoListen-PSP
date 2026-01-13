#pragma once

// FROM https://github.com/richgel999/picojpeg/blob/master/jpg2tga.c

#include "picojpeg.h"

#include <string.h>
#include <pspkerneltypes.h>
#include <pspsysmem.h>

static const unsigned char *g_pMemBuf;
static unsigned int g_nMemBufSize;
static unsigned int g_nMemBufOfs;

static unsigned char pjpeg_mem_need_bytes_callback(unsigned char *pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data) {
   unsigned int n;
   n = g_nMemBufSize - g_nMemBufOfs;
   if (n > buf_size) n = buf_size;
   if (n) memcpy(pBuf, g_pMemBuf + g_nMemBufOfs, n);
   *pBytes_actually_read = (unsigned char) n;
   g_nMemBufOfs += n;
   return 0;
}

SceUID pjpeg_decode_from_memory(const unsigned char *pJpegData, unsigned int jpegSize, int *pWidth, int *pHeight, int *pComps) {
   pjpeg_image_info_t image_info;
   unsigned char status;
   unsigned char *pImage;
   unsigned int row_pitch;
   int mcu_x = 0, mcu_y = 0;

   *pWidth = 0;
   *pHeight = 0;
   *pComps = 0;

   g_pMemBuf = pJpegData;
   g_nMemBufSize = jpegSize;
   g_nMemBufOfs = 0;

   status = pjpeg_decode_init( & image_info, pjpeg_mem_need_bytes_callback, NULL, 0);
   if (status)
      return -1;

   row_pitch = image_info.m_width * image_info.m_comps;
   SceUID allocated = sceKernelAllocPartitionMemory(2, "jpeg_dec_buf", PSP_SMEM_High, row_pitch * image_info.m_height, NULL);
   pImage = (unsigned char *) sceKernelGetBlockHeadAddr(allocated);
   if (!pImage)
      return -1;

   for (;;) {
      int y, x;
      unsigned char *pDst_row;

      status = pjpeg_decode_mcu();
      if (status) {
         if (status != PJPG_NO_MORE_BLOCKS) {
            sceKernelFreePartitionMemory(allocated);
            return -1;
         }
         break;
      }

      pDst_row = pImage + (mcu_y * image_info.m_MCUHeight) * row_pitch + (mcu_x * image_info.m_MCUWidth * image_info.m_comps);

      for (y = 0; y < image_info.m_MCUHeight; y += 8) {
         int by_limit = image_info.m_height - (mcu_y * image_info.m_MCUHeight + y);
         if (by_limit > 8) by_limit = 8;

         for (x = 0; x < image_info.m_MCUWidth; x += 8) {
            unsigned char *pDst_block = pDst_row + x * image_info.m_comps;
            unsigned int src_ofs = (x * 8U) + (y * 16U);
            int bx_limit = image_info.m_width - (mcu_x * image_info.m_MCUWidth + x);
            int bx, by;
            if (bx_limit > 8) bx_limit = 8;

            for (by = 0; by < by_limit; by++) {
               unsigned char *pDst = pDst_block;
               for (bx = 0; bx < bx_limit; bx++) {
                  pDst[0] = image_info.m_pMCUBufR[src_ofs];
                  if (image_info.m_comps > 1) {
                     pDst[1] = image_info.m_pMCUBufG[src_ofs];
                     pDst[2] = image_info.m_pMCUBufB[src_ofs];
                  }
                  pDst += image_info.m_comps;
                  src_ofs++;
               }
               src_ofs += (8 - bx_limit);
               pDst_block += row_pitch;
            }
         }
         pDst_row += (row_pitch * 8);
      }

      mcu_x++;
      if (mcu_x == image_info.m_MCUSPerRow) {
         mcu_x = 0;
         mcu_y++;
      }
   }

   *pWidth = image_info.m_width;
   *pHeight = image_info.m_height;
   *pComps = image_info.m_comps;

   return allocated;
}