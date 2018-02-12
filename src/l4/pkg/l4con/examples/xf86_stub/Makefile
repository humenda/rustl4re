PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= pslim_drv_43.o pslim_drv_70.so pslim_drv_71.so pslim_drv_73.so
MODE		= linux
LIBS		= -lcon.p -ldm_generic.p -ldm_mem.p -lnames.p \
		  -ll4util.p $(ROOTPLIB) -ll4sys-l4x.p
LIBCINCDIR	= $(addprefix -I,$(L4INCDIR) $(GCCINCDIR))
LDFLAGS		= -nostdlib -nostartfiles
LDSCRIPT	= $(call findfile,main_rel.ld,$(L4LIBDIR))

include $(L4DIR)/mk/Makeconf

CRT0		= $(CRTP_DEFAULT)

# Set to the Xorg/XFree86 xc directory. You have to invoke "make World" in
# that directory to make sure that the xc/exports/include directory exists.
XORGTREE	?=
XF86TREE	?=

ifneq ($(XF86TREE),)
# external XFree86 4.3.x tree
INCDIR_XF86     =  $(XF86TREE)/include \
		   $(XF86TREE)/include/X11 \
		   $(XF86TREE)/include/fonts \
		   $(XF86TREE)/include/extensions \
		   $(XF86TREE)/include/xserver \
		   $(XF86TREE)/programs/Xserver/hw/xfree86/common \
		   $(XF86TREE)/programs/Xserver/hw/xfree86/os-support \
		   $(XF86TREE)/programs/Xserver/hw/xfree86/os-support/bus \
		   $(XF86TREE)/programs/Xserver/hw/xfree86/xf24_32bpp \
		   $(XF86TREE)/programs/Xserver/hw/xfree86/xaa \
		   $(XF86TREE)/programs/Xserver/hw/xfree86/shadowfb \
		   $(XF86TREE)/programs/Xserver/hw/xfree86 \
		   $(XF86TREE)/programs/Xserver/include \
		   $(XF86TREE)/programs/Xserver/fb \
		   $(XF86TREE)/programs/Xserver/mi \
		   $(XF86TREE)/programs/Xserver/afb \
		   $(XF86TREE)/programs/Xserver/mfb \
		   $(XF86TREE)/programs/Xserver/render \
		   $(XF86TREE)/programs/Xserver/miext/shadow
else
# shipped XFree86 4.3.x headers
INCDIR_XF86 =      $(SRC_DIR) \
	 	   $(SRC_DIR)/include-xf86-4.3.0 \
	 	   $(SRC_DIR)/include-xf86-4.3.0/X11 \
	 	   $(SRC_DIR)/include-xf86-4.3.0/X11/extensions \
	 	   $(SRC_DIR)/include-xf86-4.3.0/fonts \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/mi \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/xf24_32bpp \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/fb \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/afb \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/mfb \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/os-support \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/miext \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/render \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/xaa \
	 	   $(SRC_DIR)/include-xf86-4.3.0/xserver/shadowfb
endif

ifneq ($(XORG69TREE),)
# external X.org 6.9/7.0 tree
INCDIR_XORG69    = $(SRC_DIR) \
		   $(XORG69TREE)/exports/include \
		   $(XORG69TREE)/exports/include/X11 \
		   $(XORG69TREE)/exports/include/X11/extensions \
		   $(XORG69TREE)/include/fonts \
		   $(XORG69TREE)/programs/Xserver/include \
		   $(XORG69TREE)/programs/Xserver/hw/xfree86/common \
		   $(XORG69TREE)/programs/Xserver/hw/xfree86/os-support \
		   $(XORG69TREE)/programs/Xserver/hw/xfree86/xf24_32bpp \
		   $(XORG69TREE)/programs/Xserver/hw/xfree86/xaa \
		   $(XORG69TREE)/programs/Xserver/hw/xfree86/shadowfb \
		   $(XORG69TREE)/programs/Xserver/hw/xfree86 \
		   $(XORG69TREE)/programs/Xserver/fb \
		   $(XORG69TREE)/programs/Xserver/mi \
		   $(XORG69TREE)/programs/Xserver/afb \
		   $(XORG69TREE)/programs/Xserver/mfb \
		   $(XORG69TREE)/programs/Xserver/render \
		   $(XORG69TREE)/programs/Xserver/miext/shadow
else
# shipped X.org 6.9/7.0 headers
INCDIR_XORG69 =	   $(SRC_DIR) \
		   $(SRC_DIR)/include-xorg-6.9.0 \
		   $(SRC_DIR)/include-xorg-6.9.0/X11 \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/hw/xfree86 \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/hw/xfree86/common \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/hw/xfree86/os-support \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/hw/xfree86/os-support/bus \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/hw/xfree86/xaa \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/hw/xfree86/shadowfb \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/miext/shadow \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/fb \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/afb \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/mfb \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/mi \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/miext \
		   $(SRC_DIR)/include-xorg-6.9.0/xserver/render
endif

# shipped X.org 7.1 headers
INCDIR_XORG71 =	   $(SRC_DIR) \
		   $(SRC_DIR)/include-xorg-7.1.0
INCDIR_XORG73 =	   $(SRC_DIR) \
		   $(SRC_DIR)/include-xorg-7.3.0

DEFINES_pslim_43.c        = -Dlinux -D__i386__ -D_POSIX_C_SOURCE=199309L \
			    -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE \
			    -D_SVID_SOURCE -D_GNU_SOURCE -DSHAPE -DXINPUT \
			    -DXKB -DLBX -DXAPPGROUP -DXCSECURITY -DTOGCUP \
			    -DXF86BIGFONT -DDPMSExtension -DPIXPRIV \
			    -DPANORAMIX -DRENDER -DGCCUSESGAS \
			    -DAVOID_GLYPHBLT -DPIXPRIV -DSINGLEDEPTH \
			    -DXFreeXDGA -DXvExtension -DXFree86LOADER \
			    -DXFree86Server -DXF86VIDMODE -DSMART_SCHEDULE \
			    -DBUILDDEBUG -DX_BYTE_ORDER=X_LITTLE_ENDIAN \
			    -DNDEBUG -DFUNCPROTO=15 -DNARROWPROTO -DIN_MODULE \
			    -DXFree86Module
DEFINES_emul_l4env_43.c  := $(DEFINES_pslim_43.c)
DEFINES_emul_l4env_70.c  := $(DEFINES_emul_l4env_43.c)
DEFINES_emul_l4env_71.c  := $(DEFINES_emul_l4env_43.c)
DEFINES_emul_l4env_73.c  := $(DEFINES_emul_l4env_43.c) -DXORG73

DEFINES_pslim_70.c       := $(DEFINES_pslim_43.c) \
			    -DL4CON_USE_DRIVER_FUNC -DXORG70
DEFINES_pslim_71.c       := $(DEFINES_pslim_43.c) \
			    -DL4CON_USE_DRIVER_FUNC -DXORG71
DEFINES_pslim_73.c       := $(DEFINES_pslim_43.c) \
			    -DL4CON_USE_DRIVER_FUNC -DXORG71 -DXORG73
DEFINES_pslim_43.c       += -DL4CON_USE_CFB24_32

# don't include Linux headers!
CPPFLAGS		       = -nostdinc
CFLAGS			       = -fPIC

LDFLAGS_pslim_drv_43.o        += -r
LDFLAGS_pslim_drv_70.so       += -shared
LDFLAGS_pslim_drv_71.so       += -shared
LDFLAGS_pslim_drv_73.so       += -shared

DEFINES_helper_43.c            = -DXFree86LOADER -DIN_MODULE
DEFINES_helper_70.c            = -DXFree86LOADER -DIN_MODULE
DEFINES_helper_71.c            = -DXFree86LOADER -DIN_MODULE
DEFINES_helper_73.c            = -DXFree86LOADER -DIN_MODULE

PRIVATE_INCDIR_pslim_43.o      = $(INCDIR_XF86)
PRIVATE_INCDIR_helper_43.o     = $(INCDIR_XF86)
PRIVATE_INCDIR_emul_l4env_43.o = $(INCDIR_XF86)

PRIVATE_INCDIR_pslim_70.o      = $(INCDIR_XORG69)
PRIVATE_INCDIR_helper_70.o     = $(INCDIR_XORG69)
PRIVATE_INCDIR_emul_l4env_70.o = $(INCDIR_XORG69)

PRIVATE_INCDIR_pslim_71.o      = $(INCDIR_XORG71)
PRIVATE_INCDIR_helper_71.o     = $(INCDIR_XORG71)
PRIVATE_INCDIR_emul_l4env_71.o = $(INCDIR_XORG71)

PRIVATE_INCDIR_pslim_73.o      = $(INCDIR_XORG73) \
				 $(patsubst -I%,%,$(LIBC_INCLUDE))
PRIVATE_INCDIR_helper_73.o     = $(INCDIR_XORG73) \
				 $(patsubst -I%,%,$(LIBC_INCLUDE))
PRIVATE_INCDIR_emul_l4env_73.o = $(INCDIR_XORG73) \
				 $(patsubst -I%,%,$(LIBC_INCLUDE))

# !!! Please don't link against -l4env-l4lx. Need special compiler flags !!!
# !!! for emul_l4env.c (mmap => xf86mmap).                               !!!
SRC_C_pslim_drv_43.o           = pslim_43.c helper_43.c emul_l4env_43.c
SRC_C_pslim_drv_70.so          = pslim_70.c helper_70.c emul_l4env_70.c
SRC_C_pslim_drv_71.so          = pslim_71.c helper_71.c emul_l4env_71.c
SRC_C_pslim_drv_73.so          = pslim_73.c helper_73.c emul_l4env_73.c

.general.d: $(SRC_C_pslim_drv_43.o) \
	    $(SRC_C_pslim_drv_70.so) \
	    $(SRC_C_pslim_drv_71.so) \
	    $(SRC_C_pslim_drv_73.so)

%_43.c: %.c
	@ln -s $< $@

%_70.c: %.c
	@ln -s $< $@

%_71.c: %.c
	@ln -s $< $@

%_73.c: %.c
	@ln -s $< $@

INSTALLFILE_BIN_LOCAL = $(STRIP) --strip-unneeded $(1) \
			-o $(INSTALLDIR_LOCAL)/$(1)

include $(L4DIR)/mk/prog.mk
