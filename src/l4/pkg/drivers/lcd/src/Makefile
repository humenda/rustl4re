PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

SRC_C          := lcd.c lcd-virtual.c lcd-amba.c lcd-s3c2410.c lcd-omap3.c #lcd-h3800.c
SRC_C          += lcd-tegra2.c
TARGET		= libarm_lcd.o.a
PC_FILENAME    := libdrivers-lcd
REQUIRES_LIBS  := libio l4re_c l4re_c-util libdrivers-generic

PRIVATE_INCDIR	+= $(PKGDIR)/lcd/src

include $(L4DIR)/mk/lib.mk
