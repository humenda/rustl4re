# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the linux arm-specific parts of the memory manager.
#

obj-y				:= extable.o fault.o init.o iomap.o
obj-y				+= dma-mapping$(MMUEXT).o
obj-$(CONFIG_MMU)		+= fault-armv.o flush.o idmap.o ioremap.o \
				   mmap.o pgd.o mmu.o pageattr.o
KASAN_SANITIZE_mmu.o		:= n

obj-$(CONFIG_L4)		+= proc-l4.o

ifneq ($(CONFIG_MMU),y)
obj-y				+= nommu.o
obj-$(CONFIG_ARM_MPU)		+= pmsa-v7.o pmsa-v8.o
endif

obj-$(CONFIG_ARM_PTDUMP_CORE)	+= dump.o
obj-$(CONFIG_ARM_PTDUMP_DEBUGFS)	+= ptdump_debugfs.o
obj-$(CONFIG_MODULES)		+= proc-syms.o
KASAN_SANITIZE_physaddr.o	:= n
obj-$(CONFIG_DEBUG_VIRTUAL)	+= physaddr.o

obj-$(CONFIG_ALIGNMENT_TRAP)	+= alignment.o

obj-$(CONFIG_CPU_COPY_V6)	+= context.o

obj-$(CONFIG_OUTER_CACHE)	+= l2c-common.o

KASAN_SANITIZE_kasan_init.o	:= n
obj-$(CONFIG_KASAN)		+= kasan_init.o

KBUILD_CPPFLAGS += -I$(srctree)/arch/arm/mm

include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../../arm/mm,       \
       dump extable fault-armv idmap iomap \
       l2c-common mmap pageattr proc-syms)
