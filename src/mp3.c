#pragma once

// FROM https://github.com/RegularRabbit05/mp3p/blob/master/include/mp3.h

#include <pspmp3.h>
#include <pspaudio.h>
#include <pspkerneltypes.h>
#include <pspsysmem.h>
#include <pspiofilemgr_fcntl.h>
#include "jpg.c"

#define MP3_MP3_BUF (16*1024)
#define MP3_PCM_BUF (16*(1152/2))
#define MP3_USE_THREAD 1

typedef struct {
   SceMp3InitArg mp3Init;
   SceUID fd;
	SceUID icon;
	SceBool paused;
	SceBool error;
	SceBool looping;
	SceBool over;
   int32_t handle;
	int32_t numPlayed;
	int32_t samplingRate;
	int32_t maxSample;
	int32_t playedSamples;
	int32_t totalSamples;
	int32_t numChannels;
	int32_t channel;
	char title[256];
	char author[256];
   unsigned char pcmBuf[MP3_PCM_BUF] __attribute__((aligned(64)));
	unsigned char mp3Buf[MP3_MP3_BUF] __attribute__((aligned(64)));
	int32_t thread;
} MP3Instance;

int mp3_read_title_author(SceUID fd, MP3Instance *meta) {
    if (fd < 0 || !meta) return -1;

    if (sceIoLseek(fd, -128, PSP_SEEK_END) < 0) {
        return -2;
    }

    unsigned char tag[128];
    if (sceIoRead(fd, tag, 128) != 128) {
        return -3;
    }

    if (memcmp(tag, "TAG", 3) != 0) {
        return -4;
    }

    strncpy(meta->title, (char *)(tag + 3), 30);
    meta->title[30] = '\0';

    strncpy(meta->author, (char *)(tag + 33), 30);
    meta->author[30] = '\0';

    return 0;
}

int mp3_read_title_author_v2(SceUID fd, MP3Instance *meta) {
    if (fd < 0 || !meta) return -1;

    if (sceIoLseek(fd, 0, PSP_SEEK_SET) < 0) {
        return -2;
    }

    unsigned char header[10];
    if (sceIoRead(fd, header, 10) != 10) {
        return -3;
    }

    if (memcmp(header, "ID3", 3) != 0) {
        return -4;
    }

    int tag_size = ((header[6] & 0x7F) << 21) |
                   ((header[7] & 0x7F) << 14) |
                   ((header[8] & 0x7F) << 7)  |
                   (header[9] & 0x7F);

    long tag_end = sceIoLseek(fd, 0, PSP_SEEK_CUR) + tag_size;

    while (sceIoLseek(fd, 0, PSP_SEEK_CUR) < tag_end) {
        unsigned char frame_header[10];
        if (sceIoRead(fd, frame_header, 10) != 10) break;
        char frame_id[5] = {0};
        memcpy(frame_id, frame_header, 4);

        uint32_t frame_size = (frame_header[4] << 24) |
                              (frame_header[5] << 16) |
                              (frame_header[6] << 8)  |
                               frame_header[7];

        if (frame_size == 0 || frame_size > 1024) {
            sceIoLseek(fd, frame_size, PSP_SEEK_CUR);
            continue;
        }

        unsigned char frame_data[1025] = {0};
        if (sceIoRead(fd, frame_data, frame_size) != frame_size) break;

        char *dest = NULL;
        if (strcmp(frame_id, "TIT2") == 0) {
            dest = meta->title;
        } else if (strcmp(frame_id, "TPE1") == 0) {
            dest = meta->author;
        }

        if (dest) {
            int encoding = frame_data[0];
            if (encoding == 0 || encoding == 3) {
                strncpy(dest, (char *)(frame_data + 1), 255);
                dest[255] = 0;
			} else {
                dest[0] = 0;
            }
        }

        if (meta->title[0] && meta->author[0]) {
            break;
        }
    }

    return 0;
}

uint32_t mp3_findStreamStart(SceUID fd, uint8_t* tmp_buf){
   uint8_t* buf = (uint8_t*) tmp_buf;
	sceIoRead(fd, buf, MP3_MP3_BUF/2);
    if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
        uint32_t header_size = (buf[9] | (buf[8]<<7) | (buf[7]<<14) | (buf[6]<<21));
        return header_size+10;
    } else if (buf[0] == 'A' && buf[1] == 'P' && buf[2] == 'E') {
        uint32_t header_size = (buf[12] | (buf[13]<<8) | (buf[14]<<16) | (buf[15]<<24));
        return header_size+32;
    }
    return 0;
}

SceBool mp3_fillStreamBuffer(SceUID fd, int handle, MP3Instance * instance) {
	unsigned char* dst;
	long int write;
	long int pos;
	int status = sceMp3GetInfoToAddStreamData(handle, &dst, &write, &pos);
	if (status < 0) {
		return 0;
	}

	status = sceIoLseek(fd, pos, PSP_SEEK_SET);
	if (status < 0) {
		return 0;
	}
	
	int read = sceIoRead(fd, dst, write);
	if (read < 0) {
      return 0;
	}
	
	if (read == 0) {
		return 0;
	}
	
	status = sceMp3NotifyAddStreamData(handle, read);
	if (status < 0) {
		return 0;
	}
	
	return (pos > 0);
}

SceBool mp3_feed(MP3Instance * mp3Instance) {
	if (mp3Instance == NULL || mp3Instance->error) return 1;
	if (mp3Instance->paused) {
		sceKernelDelayThreadCB(10000);
		return 0;
	}

    if (sceMp3CheckStreamDataNeeded(mp3Instance->handle) > 0) {
		mp3_fillStreamBuffer(mp3Instance->fd, mp3Instance->handle, mp3Instance);
    }

   short* buf;
   int bytesDecoded;
	for (int retries = 0; retries < 1; retries++) {
		bytesDecoded = sceMp3Decode(mp3Instance->handle, &buf);
		if (bytesDecoded>0) break;
		if (sceMp3CheckStreamDataNeeded(mp3Instance->handle) <= 0) break;
		if (!mp3_fillStreamBuffer(mp3Instance->fd, mp3Instance->handle, mp3Instance)) {
			mp3Instance->numPlayed = 0;
		}
	}
	
    if (bytesDecoded < 0 && ((uint32_t) bytesDecoded) != 0x80671402) {
		mp3Instance->error = 1;
		return 1;
	}

	if (bytesDecoded==0 || ((uint32_t) bytesDecoded) == 0x80671402) {
		mp3Instance->paused = 1;
		mp3Instance->over = 1;
	   sceMp3ResetPlayPosition(mp3Instance->handle);
		mp3Instance->numPlayed = 0;
		return 1;
	} else {
		mp3Instance->numPlayed += sceAudioSRCOutputBlocking(0x8000, buf);
	}
	return 0;
}

SceBool mp3_tick(MP3Instance * mp3Instance) {
	if (!MP3_USE_THREAD) mp3_feed(mp3Instance);
	return mp3Instance == NULL || mp3Instance->error || mp3Instance->over;
}

int mp3_thread(SceSize args, void* argp) {
	MP3Instance * mp3Instance = *(MP3Instance **) argp;
	while (!mp3Instance->over && mp3Instance->error == 0) {
		sceKernelDelayThreadCB(5000);
		mp3_feed(mp3Instance);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

SceUID mp3_beginPlayback(char * file) {
   SceUID memid = sceKernelAllocPartitionMemory(2, "mp3_instance", PSP_SMEM_High, sizeof(MP3Instance), NULL);
   if (memid < 0) {
      return memid;
   }
   MP3Instance * mp3Instance = (MP3Instance *) sceKernelGetBlockHeadAddr(memid);
   memset(mp3Instance, 0, sizeof(MP3Instance));
	mp3Instance->icon = -1;
   mp3Instance->fd = sceIoOpen(file, PSP_O_RDONLY, 0777);
	if (mp3Instance->fd < 0) {
		sceKernelFreePartitionMemory(memid);
      return -1;
	}
   sceIoLseek(mp3Instance->fd, 0, PSP_SEEK_SET);

	mp3Instance->mp3Init.mp3StreamEnd = sceIoLseek(mp3Instance->fd, 0, PSP_SEEK_END);
	sceIoLseek(mp3Instance->fd, 0, PSP_SEEK_SET);
	mp3Instance->mp3Init.mp3StreamStart = mp3_findStreamStart(mp3Instance->fd, mp3Instance->mp3Buf);
	mp3Instance->mp3Init.mp3Buf = mp3Instance->mp3Buf;
	mp3Instance->mp3Init.mp3BufSize = MP3_MP3_BUF;
	mp3Instance->mp3Init.pcmBuf = mp3Instance->pcmBuf;
	mp3Instance->mp3Init.pcmBufSize = MP3_PCM_BUF;

	int status = sceMp3InitResource();
	if (status < 0) {
		sceIoClose(mp3Instance->fd);
		sceKernelFreePartitionMemory(memid);
		return -2;
	}

   mp3Instance->handle = sceMp3ReserveMp3Handle(&mp3Instance->mp3Init);
	if (mp3Instance->handle < 0) {
		sceMp3TermResource();
		sceIoClose(mp3Instance->fd);
		sceKernelFreePartitionMemory(memid);
      return -3;
	}

	mp3_fillStreamBuffer(mp3Instance->fd, mp3Instance->handle, mp3Instance);

   status = sceMp3Init(mp3Instance->handle);
	if (status < 0) {
		sceMp3ReleaseMp3Handle(mp3Instance->handle);
		sceMp3TermResource();
		sceIoClose(mp3Instance->fd);
		sceKernelFreePartitionMemory(memid);
      return -4;
	}

	int oldPtr = sceIoLseek32(mp3Instance->fd, 0, PSP_SEEK_SET);
	int currentThread = sceKernelGetThreadId();
	int currentPriority = sceKernelGetThreadCurrentPriority();
	sceKernelChangeThreadPriority(currentThread, 1);
	mp3Instance->icon = readPictureFromFileMP3(mp3Instance->fd);
	sceKernelChangeThreadPriority(currentThread, currentPriority);

	mp3_read_title_author(mp3Instance->fd, mp3Instance);
	if (mp3Instance->title[0] == 0 && mp3Instance->author[0] == 0) mp3_read_title_author_v2(mp3Instance->fd, mp3Instance);
	if (mp3Instance->title[0] == 0) {
		char fileNameBuf[256];
		strncpy(fileNameBuf, file, sizeof(fileNameBuf)-1);
		fileNameBuf[sizeof(fileNameBuf)-1] = 0;
		char* ext = strrchr(fileNameBuf, '.');
		if (ext != NULL) *ext = 0;
		char* baseName = strrchr(fileNameBuf, '/');
		if (baseName != NULL) baseName++; else baseName = fileNameBuf;
		strncpy(mp3Instance->title, baseName, 255);
		mp3Instance->title[255] = 0;
	}
	sceIoLseek32(mp3Instance->fd, oldPtr, PSP_SEEK_SET);

	sceAudioSRCChRelease();
	sceMp3SetLoopNum(mp3Instance->handle, 0);

	mp3Instance->samplingRate = sceMp3GetSamplingRate(mp3Instance->handle);
	mp3Instance->numChannels = sceMp3GetMp3ChannelNum(mp3Instance->handle);
	mp3Instance->maxSample = sceMp3GetMaxOutputSample(mp3Instance->handle);
	mp3Instance->totalSamples = sceMp3GetFrameNum(mp3Instance->handle) * mp3Instance->maxSample;
	mp3Instance->channel = sceAudioSRCChReserve(mp3Instance->maxSample, mp3Instance->samplingRate, mp3Instance->numChannels);

	if (MP3_USE_THREAD) mp3Instance->thread = sceKernelCreateThread("mp3_play_thread", (SceKernelThreadEntry) mp3_thread, 0x1F, 0x800, PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU, NULL);
	if (MP3_USE_THREAD) sceKernelStartThread(mp3Instance->thread, sizeof(MP3Instance*), &mp3Instance);
   return memid;
}

static inline SceUID mp3_loadWrapper(char* file) {
	//const int v = sceKernelGetCompiledSdkVersion();
   //sceKernelSetCompiledSdkVersion(0x06060010); //Go!Explore doesn't like that
   const SceUID instance = mp3_beginPlayback(file);
	//sceKernelSetCompiledSdkVersion(v);
	return instance;
}

void mp3_stopPlayback(SceUID memid) {
   if (memid < 0) return;
   MP3Instance * mp3Instance = (MP3Instance *) sceKernelGetBlockHeadAddr(memid);
	if (mp3Instance == NULL) return;
	mp3Instance->over = 1;
	mp3Instance->paused = 1;
	if (MP3_USE_THREAD) sceKernelWaitThreadEnd(mp3Instance->thread, NULL);
	if (mp3Instance->icon >= 0) sceKernelFreePartitionMemory(mp3Instance->icon);
	mp3Instance->icon = -1;

	if (mp3Instance->channel >= 0) while (sceAudioSRCChRelease() < 0) {
		sceKernelDelayThreadCB(100);
	}
	
	sceMp3ReleaseMp3Handle(mp3Instance->handle);
	sceMp3TermResource();
   sceIoClose(mp3Instance->fd);
	sceKernelFreePartitionMemory(memid);
}
