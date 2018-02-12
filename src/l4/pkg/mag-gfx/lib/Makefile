PKGDIR		?= ..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libmag-gfx.a libmag-gfx.so
SRC_CC		= canvas.cc factory.cc
BLIT_CC_x86	= blit-x86.cc
BLIT_CC_generic	= blit.cc
SRC_CC		+= $(firstword $(BLIT_CC_$(ARCH)) $(BLIT_CC_generic))

include $(L4DIR)/mk/lib.mk
