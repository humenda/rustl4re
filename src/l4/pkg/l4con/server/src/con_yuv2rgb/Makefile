PKGDIR		?= ../../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libcon_yuv2rgb.a
SYSTEMS		= $(SYSTEMS_PLAIN)
NOTARGETSTOINSTALL = 1

SRC_C_x86	= cpu_accel.c yuv2rgb.c yuv2rgb_mmx.c
SRC_C_amd64	= $(SRC_C_x86)
SRC_C_arm	= yuv2rgb.c
SRC_C_arm64	= yuv2rgb.c
SRC_C_mips	= yuv2rgb.c
SRC_C_ppc32	= yuv2rgb.c
SRC_C_sparc	= yuv2rgb.c
SRC_C		= $(SRC_C_$(ARCH))

include $(L4DIR)/mk/lib.mk
