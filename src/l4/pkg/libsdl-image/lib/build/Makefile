PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libsdl-image.a
SYSTEMS         = $(SYSTEMS_PLAIN)
REQUIRES_LIBS   = libsdl libpng libjpeg

SRC_C           = IMG.c IMG_bmp.c IMG_gif.c IMG_jpg.c IMG_lbm.c \
                  IMG_pcx.c IMG_png.c IMG_pnm.c IMG_tga.c IMG_tif.c \
		  IMG_xcf.c IMG_xpm.c IMG_xv.c IMG_xxx.c

DEFINES		= -DLOAD_BMP -DLOAD_JPG -DLOAD_PNG -DLOAD_XPM
PRIVATE_INCDIR	= $(PKGDIR)/contrib/
CONTRIB_INCDIR	= libsdl-image
WARNINGS        = -Wno-unused

vpath %.c $(PKGDIR)/contrib/

include $(L4DIR)/mk/lib.mk
