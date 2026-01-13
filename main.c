#include <pspkernel.h>
#include <pspsdk.h>
#include <psptypes.h>
#include <pspaudio.h>
#include <pspaudiolib.h>
#include <pspdisplay.h>
#include <pspjpeg.h>
#include <psputility_modules.h>
#include <pspsysmem.h>
#include <psprtc.h>

#include "src/render.c"
#include "src/mp3.c"
#include "src/include/llsc-atomic.h"
#include "src/music.c"

#define SQR_COLOR 0x4b4b4b
#define BG_COLOR  SQR_COLOR
#define TT_COLOR  0xFFFFFF
#define AT_COLOR  0xd4d4d4

PSP_MODULE_INFO("GO_LISTEN", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
#define EXPORT __attribute__((used))

static volatile u32 running;
static volatile SceUID instance;
static volatile u32 comMutex;
static volatile u32 changeTrack;

typedef struct {
   u32 icon[64 * 64];
   char title[64];
   char author[64];
   SceBool hasMutex, xAppearing;
   s16 xOffset;
   u64 nextMovement;
} UIState;

int ui_thread(SceSize args, void *argp) {
   UIState uiState;
   memset(&uiState, 0, sizeof(UIState));
   memcpy(uiState.icon, noMusicLogoStaticData, sizeof(uiState.icon));
   uiState.xOffset = 1024;
   pspDebugSetXOffset(uiState.xOffset);

   sceKernelDelayThreadCB(14000000);
   while (atomicRead(&running) == 1) {
      if (atomicRead(&changeTrack) != 0) {
         if (uiState.hasMutex == 0) uiState.hasMutex = mutexTrylock(&comMutex);
         if (uiState.hasMutex != 0) {
            atomicWrite(&changeTrack, 0);

            if (instance >= 0) {
               MP3Instance * mp3Instance = (MP3Instance *) sceKernelGetBlockHeadAddr(instance);
               if (mp3Instance != NULL) {
                  strncpy(uiState.title, mp3Instance->title, sizeof(uiState.title)-1);
                  strncpy(uiState.author, mp3Instance->author, sizeof(uiState.author)-1);
                  uiState.title[14] = 0;
                  uiState.author[14] = 0;
                  SceBool hasImage = 0;
                  if (mp3Instance->icon >= 0) {
                     u32* iconData = sceKernelGetBlockHeadAddr(mp3Instance->icon);
                     if (iconData != NULL) {
                        memcpy(uiState.icon, iconData, sizeof(uiState.icon));
                        hasImage = 1;
                     }
                  }
                  if (!hasImage) {
                     memcpy(uiState.icon, noMusicLogoStaticData, sizeof(uiState.icon));
                  }
                  uiState.xOffset = 200 - 4;
                  uiState.xAppearing = 1;
               }
            }

            uiState.hasMutex = 0;
            mutexUnlock(&comMutex);
         }
      }

      if (uiState.xOffset >= 1024) {
         sceKernelDelayThreadCB(1000);
         sceDisplayWaitVblankStartCB();
         continue;
      }

      u64 tick = 0;
      if (sceRtcGetCurrentTick(&tick) == 0 && tick >= uiState.nextMovement) {
         if (uiState.nextMovement == 0) {
            uiState.xOffset += uiState.xAppearing ? -1 : 1;
         } else {
            float diff = tick - uiState.nextMovement * 0.01f;
            if (diff > 16.0f) diff = 16.0f;
            uiState.xOffset += (s16)(diff) * (uiState.xAppearing ? -1 : 1);
         }
         uiState.nextMovement = tick + 33000;
         if (uiState.xAppearing) {
            if (uiState.xOffset <= 0) {
               uiState.xOffset = -1024; uiState.xAppearing = 0;
            }
         } else if (uiState.xOffset > 200 - 4) uiState.xOffset = 1024;
      }

      pspDebugSetXOffset(uiState.xOffset <= 0 ? 0 : uiState.xOffset);
      sceKernelDelayThreadCB(200);
      pspDebugDrawRectangle(300 + 4, 247 - 64, 200 - 4, 64, SQR_COLOR);
      for (u8 y = 0; y < 64; y++) {
         for (u8 x = 0; x < 64; x++) {
            const u32 color = uiState.icon[y * 64 + x];
            if ((color & 0xFF000000) != 0) pspDebugDrawPixel(300 + 4 + x, 247 - 64 + y, color);
         }
      }
      blit_string(300 + 64 + 12, 247 - 64 + 26, uiState.title, TT_COLOR, BG_COLOR);
      blit_string(300 + 64 + 12, 247 - 64 + 36, uiState.author, AT_COLOR, BG_COLOR);

      deinit_vram();
      sceDisplayWaitVblankStartCB();
   }

   if (uiState.hasMutex) mutexUnlock(&comMutex);
   sceKernelExitDeleteThread(0);
   return 0;
}

int work_thread(SceSize args, void *argp) {
   sceKernelDelayThreadCB(6000000);

   MusicNode* musicListHead = NULL;
   MusicNode* currentNode = NULL;
   int trackCount = 0;
   char* path = "ms0:/MUSIC/";
   SceIoDirent dir;
   memset(&dir, 0, sizeof(SceIoDirent));
   SceUID dfd = sceIoDopen(path);
   if (dfd >= 0) {
      while (sceIoDread(dfd, &dir) > 0) {
         const char* ext = strrchr(dir.d_name, '.');
         if (ext == NULL) continue;
         if (strcicmp(ext, ".mp3") != 0) continue;
         char nameBuffer[256];
         strcpy(nameBuffer, path);
         strcat(nameBuffer, dir.d_name);
         MusicNode* newNode = createMusicNode(nameBuffer);
         if (newNode != NULL) {
            appendMusicNode(&musicListHead, newNode);
            trackCount++;
         }
      }
      sceIoDclose(dfd);
   } else {
      sceIoMkdir(path, 0777);
      sceKernelExitThread(0);
      return 0;
   }
   if (musicListHead == NULL) {
      sceKernelExitThread(0);
      return 0;
   }

   pspAudioInit();
   sceJpegInitMJpeg();

   sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
   sceUtilityLoadModule(PSP_MODULE_AV_MP3);

   int errorCount = 0;
   while (atomicRead(&running) == 1) {
      sceKernelDelayThreadCB(10000);
      mutexLock(&comMutex);
      if (instance <= 0) {
         errorCount++;
         if (errorCount >= trackCount) {
            errorCount = -1;
         } else {
            if (currentNode == NULL) {
               currentNode = musicListHead;
            } else {
               currentNode = currentNode->next;
               if (currentNode == NULL) currentNode = musicListHead;
            }
            instance = mp3_loadWrapper(currentNode->name);
            if (instance >= 0) {
               atomicWrite(&changeTrack, 1);
               errorCount = 0;
            } else {
               atomicWrite(&changeTrack, 0);
               instance = -1;
            }
         }
      }
      if (instance >= 0) {
         MP3Instance * mp3Instance = (MP3Instance *) sceKernelGetBlockHeadAddr(instance);
         if (mp3Instance != NULL) {
            SceBool over = mp3_tick(mp3Instance);
            if (over) {
               mp3_stopPlayback(instance);
               instance = -1;
            }
         } else {
            mp3_stopPlayback(instance);
            instance = -1;
         }
      }
      mutexUnlock(&comMutex);

      if (errorCount == -1) {
         atomicWrite(&running, 0);
         break;
      }
   }

   if (instance >= 0) {
      atomicWrite(&changeTrack, 0);
      mutexLock(&comMutex);
      mp3_stopPlayback(instance);
      instance = -1;
      mutexUnlock(&comMutex);
   }
   freeMusicList(musicListHead);
   sceJpegFinishMJpeg();
   sceKernelExitDeleteThread(0);
   return 0;
}

EXPORT int module_start(SceSize args, void *argp) {
   instance = -1;
   atomicWrite(&changeTrack, 0);
   atomicWrite(&comMutex, 0);
   atomicWrite(&running, 1);

   {
      int thid = sceKernelCreateThread("go_listen_ex", work_thread, 0x10, 0x2000, 0, NULL);
      if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
   }
   {
      int thid = sceKernelCreateThread("go_listen_ui", ui_thread, 0x11, 0x8000, 0, NULL);
      if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
   }
   return 0;
}

EXPORT int module_stop(SceSize args, void *argp) {
   atomicWrite(&running, 0);
   return 0;
}