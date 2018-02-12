PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libarm_nand.o.a
PC_FILENAME    := libdrivers-nand

SRC_CC		:= lib_nand.cc nand.cc nand_ids.cc gpmc.cc mpc5121.cc 

PRIVATE_INCDIR	:= $(SRCDIR)
#CFLAGS		+= -D__KERNEL__ -DHAVE_NET_DEVICE_OPS

include $(L4DIR)/mk/lib.mk
