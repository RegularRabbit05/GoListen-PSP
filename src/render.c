#pragma once
/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * scr_printf.c - Debug screen functions.
 *
 * Copyright (c) 2005 Marcus R. Brown <mrbrown@ocgnet.org>
 * Copyright (c) 2005 James Forshaw <tyranid@gmail.com>
 * Copyright (c) 2005 John Kelley <ps2dev@kelley.ca>
 *
 * $Id: scr_printf.c 2017 2006-10-07 16:51:57Z tyranid $
 */
#include <string.h>
#include <psptypes.h>
#include <pspdisplay.h>

#define PSP_SCREEN_WIDTH 480
#define PSP_SCREEN_HEIGHT 272
#define PSP_LINE_SIZE 512

static s16 xOffs = 0;
static int X = 0, Y = 0;
static u32 bg_col = 0, fg_col = 0xFFFFFFFF;
static void* g_vram_base = (u32 *) 0x04000000;
static int g_vram_offset = 0;
static int g_vram_mode = PSP_DISPLAY_PIXEL_FORMAT_8888;
static int vram_init = 0;

extern u8 msx[];

static void deinit_vram();
static SceBool init_vram();
static u16 convert_8888_to_565(u32 color);
static u16 convert_8888_to_5551(u32 color);
static u16 convert_8888_to_4444(u32 color);
void pspDebugScreenSetBackColor(u32 colour);
void pspDebugScreenSetTextColor(u32 colour);
void pspDebugScreenSetColorMode(int mode);
int pspDebugScreenGetX();
int pspDebugScreenGetY();
void pspDebugScreenSetXY(int x, int y);
void pspDebugScreenSetOffset(int offset);
void pspDebugScreenSetBase(u32* base);
static void debug_put_char_32(int x, int y, u32 color, u32 bgc, u8 ch);
static void debug_put_char_16(int x, int y, u16 color, u16 bgc, u8 ch);
void pspDebugScreenPutChar(int x, int y, u32 color, u8 ch);
void _pspDebugScreenClearLine(int Y);
int pspDebugScreenPrintData(const char *buff, int size);
void pspDebugDrawPixel(int x, int y, u32 color);
void pspDebugDrawRectangle(int x, int y, int w, int h, u32 color);
static void pspDebugSetXOffset(int offset);

static void deinit_vram() {
   vram_init = 0;
}

static SceBool init_vram() {
   int pwidth, pheight, bufferwidth, pixelformat, unk;
   unsigned int* vram32;
   sceDisplayGetMode(&unk, &pwidth, &pheight);
   sceDisplayGetFrameBuf((void*)&vram32, &bufferwidth, &pixelformat, unk);

   if((bufferwidth == 0) || (vram32 == 0)) return 0;

   vram_init = 1;
   pspDebugScreenSetColorMode(pixelformat);
   pspDebugScreenSetOffset(0);
   pspDebugScreenSetBase((void*)vram32);
   return 1;
}

inline static int blit_string(int sx,int sy,const char *msg,int fg_col,int bg_col) {
   if (vram_init == 0) if (init_vram() == 0) return 0;
   pspDebugScreenSetBackColor(bg_col);
   pspDebugScreenSetTextColor(fg_col);
   pspDebugScreenSetXY(sx, sy);
   const int size = strlen(msg);
   return pspDebugScreenPrintData(msg, size);
}

inline static u16 convert_8888_to_565(u32 color) {
   int r, g, b;

   b = (color >> 19) & 0x1F;
   g = (color >> 10) & 0x3F;
   r = (color >> 3) & 0x1F;

   return r | (g << 5) | (b << 11);
}

inline static u16 convert_8888_to_5551(u32 color) {
   int r, g, b, a;

   a = (color >> 24) ? 0x8000 : 0;
   b = (color >> 19) & 0x1F;
   g = (color >> 11) & 0x1F;
   r = (color >> 3) & 0x1F;

   return a | r | (g << 5) | (b << 10);
}

inline static u16 convert_8888_to_4444(u32 color) {
   int r, g, b, a;

   a = (color >> 28) & 0xF;
   b = (color >> 20) & 0xF;
   g = (color >> 12) & 0xF;
   r = (color >> 4) & 0xF;

   return (a << 12) | r | (g << 4) | (b << 8);
}

inline void pspDebugScreenSetBackColor(u32 colour) {
   bg_col = colour;
}

inline void pspDebugScreenSetTextColor(u32 colour) {
   fg_col = colour;
}

inline void pspDebugScreenSetColorMode(int mode) {
   switch (mode) {
      case PSP_DISPLAY_PIXEL_FORMAT_565:
      case PSP_DISPLAY_PIXEL_FORMAT_5551:
      case PSP_DISPLAY_PIXEL_FORMAT_4444:
      case PSP_DISPLAY_PIXEL_FORMAT_8888:
         break;
      default:
         mode = PSP_DISPLAY_PIXEL_FORMAT_8888;
   };

   g_vram_mode = mode;
}

inline int pspDebugScreenGetX() {
   return X;
}

inline int pspDebugScreenGetY() {
   return Y;
}

inline void pspDebugScreenSetXY(int x, int y) {
   if (x < PSP_SCREEN_WIDTH && x >= 0) X = x;
   if (y < PSP_SCREEN_HEIGHT && y >= 0) Y = y;
}

inline void pspDebugScreenSetOffset(int offset) {
   g_vram_offset = offset;
}

inline void pspDebugScreenSetBase(u32 * base) {
   g_vram_base = base;
}

inline static void debug_put_char_32(int x, int y, u32 color, u32 bgc, u8 ch) {
   int i, j;
   u8 *font;

   if (!vram_init) if (init_vram() == 0) return;

   font = &msx[(int)ch * 8];
   for (i = 0; i < 8; i++, font++) {
      for (j = 0; j < 8; j++) {
         u32 pixel = (*font & (128 >> j)) ? color : bgc;
         pspDebugDrawPixel(x + j, y + i, pixel);
      }
   }
}

inline void pspDebugScreenPutChar(int x, int y, u32 color, u8 ch) {
   debug_put_char_32(x, y, color, bg_col, ch);
}

inline void _pspDebugScreenClearLine( int Y) {
   for (int i = 0; i < PSP_SCREEN_WIDTH; i += 7) pspDebugScreenPutChar( i , Y, bg_col, 219);
}

inline int pspDebugScreenPrintData(const char * buff, int size) {
   int i;
   int j;
   char c;

   if (!vram_init) if (init_vram() == 0) return 0;

   for (i = 0; i < size; i++) {
      c = buff[i];
      switch (c) {
      case '\n':
         X = 0;
         Y += 8;
         if (Y >= PSP_SCREEN_HEIGHT) Y = 0;
         _pspDebugScreenClearLine(Y);
         break;
      case '\t':
         for (j = 0; j < 5; j++) {
            pspDebugScreenPutChar(X, Y, fg_col, ' ');
            X += 7;
         }
         break;
      default:
         pspDebugScreenPutChar(X, Y, fg_col, c);
         X += 7;
         if (X >= PSP_SCREEN_WIDTH) {
            X = 0;
            Y += 8;
            if (Y >= PSP_SCREEN_HEIGHT) Y = 0;
            _pspDebugScreenClearLine(Y);
         }
      }
   }

   return i;
}

inline void pspDebugDrawPixel(int x, int y, u32 color) {
   x += xOffs;
   if (x >= PSP_SCREEN_WIDTH) return;
   if (!vram_init) if (init_vram() == 0) return;
   if (g_vram_mode == PSP_DISPLAY_PIXEL_FORMAT_8888) {
      u32 * vram = g_vram_base;
      vram += (g_vram_offset >> 2) + x;
      vram += (y * PSP_LINE_SIZE);
      *vram = color;
   } else {
      u16 c = 0;
      switch (g_vram_mode) {
      case PSP_DISPLAY_PIXEL_FORMAT_565:
         c = convert_8888_to_565(color);
         break;
      case PSP_DISPLAY_PIXEL_FORMAT_5551:
         c = convert_8888_to_5551(color);
         break;
      case PSP_DISPLAY_PIXEL_FORMAT_4444:
         c = convert_8888_to_4444(color);
         break;
      };
      u16 * vram = g_vram_base;
      vram += (g_vram_offset >> 1) + x;
      vram += (y * PSP_LINE_SIZE);
      *vram = c;
   }
}

inline void pspDebugDrawRectangle(int x, int y, int w, int h, u32 color) {
   x += xOffs;
   if (x >= PSP_SCREEN_WIDTH) return;
   if (x + w > PSP_SCREEN_WIDTH) w = PSP_SCREEN_WIDTH - x;
   if (!vram_init) if (init_vram() == 0) return;
   if (g_vram_mode == PSP_DISPLAY_PIXEL_FORMAT_8888) {
      u32 * vram = g_vram_base;
      vram += (g_vram_offset >> 2) + x;
      vram += (y * PSP_LINE_SIZE);
      for (int j = 0; j < h; j++) {
         for (int i = 0; i < w; i++) {
            vram[i] = color;
         }
         vram += PSP_LINE_SIZE;
      }
   } else {
      u16 c = 0;
      switch (g_vram_mode) {
      case PSP_DISPLAY_PIXEL_FORMAT_565:
         c = convert_8888_to_565(color);
         break;
      case PSP_DISPLAY_PIXEL_FORMAT_5551:
         c = convert_8888_to_5551(color);
         break;
      case PSP_DISPLAY_PIXEL_FORMAT_4444:
         c = convert_8888_to_4444(color);
         break;
      };
      u16 * vram = g_vram_base;
      vram += (g_vram_offset >> 1) + x;
      vram += (y * PSP_LINE_SIZE);
      for (int j = 0; j < h; j++) {
         for (int i = 0; i < w; i++) {
            vram[i] = c;
         }
         vram += PSP_LINE_SIZE;
      }
   }
}

static void pspDebugSetXOffset(int offset) {
   xOffs = offset;
}