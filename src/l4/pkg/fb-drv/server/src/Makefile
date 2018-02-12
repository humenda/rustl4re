PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = fb-drv
SRC_CC_x86      = vesa.cc
SRC_CC_amd64    = vesa.cc
SRC_CC_arm      = lcddrv.cc
SRC_CC          = main.cc dummy.cc splash.cc $(SRC_CC_$(ARCH))
REQUIRES_LIBS_x86   = x86emu_int10
REQUIRES_LIBS_amd64 = x86emu_int10
REQUIRES_LIBS_arm   = libdrivers-lcd
REQUIRES_LIBS       = libc_support_misc libio-vbus $(REQUIRES_LIBS_$(ARCH))

DEFINES         = -DSPLASHNAME=gimp_image \
                  -DSPLASHNAME_RUN_LENGTH_DECODE=GIMP_IMAGE_RUN_LENGTH_DECODE

CPPFLAGS_splash.cc = -include $(PKGDIR)/server/data/splash1.c

include $(L4DIR)/mk/prog.mk
