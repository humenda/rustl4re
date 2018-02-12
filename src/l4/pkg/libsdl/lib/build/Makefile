PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libSDL.a

CONTRIB_INCDIR  = libsdl
REQUIRES_LIBS   = libc_support_misc l4re_c-util libpthread
WARNINGS        = $(WARNINGS_MINIMAL) -Wno-unused-but-set-variable \
                  -Wno-implicit-function-declaration \
                  -Wno-unused-variable

SRC_C = $(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/events/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/stdlib/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(filter-out $(PKGDIR)/contrib/src/audio/SDL_audio.c, $(wildcard $(PKGDIR)/contrib/src/audio/*.c))) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(filter-out $(PKGDIR)/contrib/src/video/SDL_video.c, $(wildcard $(PKGDIR)/contrib/src/video/*.c))) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/video/dummy/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/audio/dummy/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/cdrom/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/cdrom/dummy/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/joystick/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/joystick/dummy/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/cpuinfo/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/timer/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/contrib/src/thread/*.c)) \
	$(patsubst $(PKGDIR)/%.c, %.c, $(filter-out $(PKGDIR)/contrib/src/thread/pthread/SDL_systhread.c, $(wildcard $(PKGDIR)/contrib/src/thread/pthread/*.c))) \
	lib/src/src/thread/pthread/SDL_systhread.c \
	contrib/src/file/SDL_rwops.c \
	$(patsubst $(PKGDIR)/%.c, %.c, $(wildcard $(PKGDIR)/lib/src/src/video/l4fb/*.c)) \
	lib/src/src/audio/SDL_audio.c \
	lib/src/src/audio/null/SDL_nullaudio.c \
	lib/src/src/video/SDL_video.c \
	lib/src/src/timer/linux/SDL_systimer.c

PRIVATE_INCDIR	= $(PKGDIR)/contrib/src \
		  $(PKGDIR)/contrib/src/events \
		  $(PKGDIR)/contrib/src/thread \
		  $(PKGDIR)/contrib/src/thread/pthread \
		  $(PKGDIR)/lib/src/src/audio \
		  $(PKGDIR)/contrib/src/audio \
		  $(PKGDIR)/contrib/src/cdrom \
		  $(PKGDIR)/contrib/src/joystick \
		  $(PKGDIR)/lib/src/src/video \
		  $(PKGDIR)/contrib/src/video \
		  $(PKGDIR)/contrib/src/timer \
		  $(PKGDIR)/lib/src/src/video/l4fb \
		  $(PKGDIR)/lib/src/src/audio/null

vpath %.c $(PKGDIR)

include $(L4DIR)/mk/lib.mk
