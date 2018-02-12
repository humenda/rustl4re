
lib-y		:= backtrace.o changebit.o csumipv6.o csumpartial.o   \
		   csumpartialcopy.o csumpartialcopyuser.o clearbit.o \
		   delay.o delay-loop.o findbit.o memchr.o memcpy.o   \
		   memmove.o memset.o setbit.o                        \
		   strchr.o strrchr.o                                 \
		   testchangebit.o testclearbit.o testsetbit.o        \
		   ashldi3.o ashrdi3.o lshrdi3.o muldi3.o             \
		   ucmpdi2.o lib1funcs.o div64.o                      \
		   io-readsb.o io-writesb.o io-readsl.o io-writesl.o  \
		   call_with_stack.o bswapsdi2.o

mmu-y		:= copy_page.o

# using lib_ here won't override already available weak symbols
obj-$(CONFIG_UACCESS_WITH_MEMCPY) += uaccess_with_memcpy.o

lib-$(CONFIG_MMU) += $(mmu-y)

ifeq ($(CONFIG_CPU_32v3),y)
  lib-y	+= io-readsw-armv3.o io-writesw-armv3.o
else
  lib-y	+= io-readsw-armv4.o io-writesw-armv4.o
endif

ifeq ($(CONFIG_ARCH_RPC),y)
  lib-y				+= ecard.o io-acorn.o floppydma.o
  AFLAGS_delay-loop.o		+= -march=armv4
endif

$(obj)/$(b)/csumpartialcopy.o:	$(obj)/$(b)/csumpartialcopygeneric.S
$(obj)/csumpartialcopyuser.o:	$(obj)/csumpartialcopygeneric.S

ifeq ($(CONFIG_KERNEL_MODE_NEON),y)
  NEON_FLAGS			:= -mfloat-abi=softfp -mfpu=neon
  CFLAGS_xor-neon.o		+= $(NEON_FLAGS)
  obj-$(CONFIG_XOR_BLOCKS)	+= xor-neon.o
endif

lib-y	+= ../pte.o ../memcpy_user.o ../uaccess.o

obj-y	+= ../proc.o
obj-$(CONFIG_L4_DEBUG) += ../debugfs.o

include arch/l4/Makefile.lib

lib-y := $(call L4X_PREFIX_IF, ../../../arm/lib, $(lib-y), \
           backtrace changebit csumipv6 csumpartial csumpartialcopy \
	   clearbit delay delay-loop findbit memchr memcpy \
	   memmove memset memzero setbit strchr strrchr \
	   testchangebit testclearbit testsetbit \
	   ashldi3 ashrdi3 lshrdi3 muldi3 \
	   ucmpdi2 lib1funcs div64 \
	   io-readsb io-writesb io-readsl io-writesl \
	   call_with_stack bswapsdi2 io-writesw-armv4 io-readsw-armv4 \
	   copy_page)
lib-y := $(call L4X_PREFIX_IF, .., $(lib-y), csumpartialcopyuser)
