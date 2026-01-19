TARGET = GO_LISTEN
OBJS = main.o

CFLAGS = -O3 -Os -G0
         
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = -O3 -Os
LDFLAGS = 

PRX_EXPORTS = exports.exp
PSP_FW_VERSION = 661

LIBS = -lpspdisplay -lpspge -lpspaudio -lmad -lpspmp3 -lpspjpeg -lpsprtc -lpspdebug -lpsputility -lpspuser -lpsppower

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

realclean:
	/bin/rm -f $(OBJS) $(TARGET).elf

all: realclean