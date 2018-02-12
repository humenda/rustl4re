# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the L4 architecture.
#

# BITS is used as extension for files which are available in a 32 bit
# and a 64 bit version to simplify shared Makefiles.
# e.g.: obj-y += foo_$(BITS).o
BITS := 32
export BITS

ifeq ($(LINSRCARCH),arm)
# arm selection
L4API		:= f
L4X_SRCARCH	:= arm

CHECKFLAGS	:= -D__arm__ -m32

L4_MK_ARCH	:= arm
OBJCOPYARCH	:= elf32-littlearm

else

# x86 selection
L4API		:= f
L4X_SRCARCH	:= x86

ifneq ($(CONFIG_X86_32),)
L4_MK_ARCH	:= x86
OBJCOPYARCH	:= elf32-i386
else
L4_MK_ARCH	:= amd64
OBJCOPYARCH	:= elf64-x86-64
endif

endif

PLATFORMNAME-$(CONFIG_L4_PLATFORM_GENERIC)  = generic
PLATFORMNAME-$(CONFIG_L4_PLATFORM_REALVIEW) = realview
PLATFORMNAME                                = $(PLATFORMNAME-y)

# select defconfig based on actual architecture
ifeq ($(ARCH),x86)
  ifeq ($(shell uname -m),x86_64)
        KBUILD_DEFCONFIG := x86_64_defconfig
  else
        KBUILD_DEFCONFIG := x86-native_defconfig
  endif
else
        KBUILD_DEFCONFIG := $(ARCH)_defconfig
endif

ifeq ($(L4X_SRCARCH),x86)
# For gcc stack alignment is specified with -mpreferred-stack-boundary,
# clang has the option -mstack-alignment for that purpose.
ifneq ($(call cc-option, -mpreferred-stack-boundary=4),)
      cc_stack_align4 := -mpreferred-stack-boundary=2
      cc_stack_align8 := -mpreferred-stack-boundary=3
else ifneq ($(call cc-option, -mstack-alignment=16),)
      cc_stack_align4 := -mstack-alignment=4
      cc_stack_align8 := -mstack-alignment=8
endif
endif # L4X_SRCARCH==x86

VMLINUZ_SUFFIX = $(patsubst "%",%,$(CONFIG_L4_VMLINUZ_SUFFIX))

export L4X_SRCARCH L4_MK_ARCH L4_MK_API OBJCOPYARCH VMLINUZ_SUFFIX

#HAS_BIARCH	:= $(call cc-option-yn, -m32)
#ifeq ($(HAS_BIARCH),y)
#AS		:= $(AS) --32
#LD		:= $(LD) -m elf_i386
#CC		:= $(CC) -m32
#endif

BASEENV		:= l4env

L4_MK_API	:= l4$(L4API)

# overwrite UTS_MACHINE to get proper architecture in user land
UTS_MACHINE	:= $(L4X_SRCARCH)

KBUILD_CFLAGS   += -pipe -msoft-float -DARCH_$(L4_MK_ARCH)

ifeq ($(L4X_SRCARCH),x86)

ifdef CONFIG_X86_NEED_RELOCS
        LDFLAGS_vmlinux := --emit-relocs
endif

ifneq ($(CONFIG_X86),)
#
# Prevent GCC from generating any FP code by mistake.
#
# This must happen before we try the -mpreferred-stack-boundary, see:
#
#    https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53383
#
KBUILD_CFLAGS += -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
KBUILD_CFLAGS += $(call cc-option,-mno-avx,)
endif

ifeq ($(CONFIG_X86_32),y)
        BITS := 32
        UTS_MACHINE := i386
        CHECKFLAGS += -D__i386__

        biarch := $(call cc-option,-m32)
        KBUILD_AFLAGS += $(biarch)
        KBUILD_CFLAGS += $(biarch)

        #KBUILD_CFLAGS += -msoft-float -mregparm=3 -freg-struct-return
	KBUILD_CFLAGS += -mregparm=3

        # Never want PIC in a 32-bit kernel, prevent breakage with GCC built
        # with nonstandard options
        KBUILD_CFLAGS += -fno-pic

        # Align the stack to the register width instead of using the default
        # alignment of 16 bytes. This reduces stack usage and the number of
        # alignment instructions.
        KBUILD_CFLAGS += $(call cc-option,$(cc_stack_align4))

        # Disable unit-at-a-time mode on pre-gcc-4.0 compilers, it makes gcc use
        # a lot more stack due to the lack of sharing of stacklots:
        KBUILD_CFLAGS += $(call cc-ifversion, -lt, 0400, \
				$(call cc-option,-fno-unit-at-a-time))

        # CPU-specific tuning. Anything which can be shared with UML should go here.
        include arch/x86/Makefile_32.cpu
        KBUILD_CFLAGS += $(cflags-y)

        # temporary until string.h is fixed
        KBUILD_CFLAGS += -ffreestanding
else
        BITS := 64
        UTS_MACHINE := x86_64
        CHECKFLAGS += -D__x86_64__ -m64

        biarch := -m64
        KBUILD_AFLAGS += -m64
        KBUILD_CFLAGS += -m64

        # Align jump targets to 1 byte, not the default 16 bytes:
        KBUILD_CFLAGS += $(call cc-option,-falign-jumps=1)

        # Pack loops tightly as well:
        KBUILD_CFLAGS += $(call cc-option,-falign-loops=1)

        # Don't autogenerate traditional x87 instructions
        KBUILD_CFLAGS += $(call cc-option,-mno-80387)
        KBUILD_CFLAGS += $(call cc-option,-mno-fp-ret-in-387)

        # By default gcc and clang use a stack alignment of 16 bytes for x86.
        # However the standard kernel entry on x86-64 leaves the stack on an
        # 8-byte boundary. If the compiler isn't informed about the actual
        # alignment it will generate extra alignment instructions for the
        # default alignment which keep the stack *mis*aligned.
        # Furthermore an alignment to the register width reduces stack usage
        # and the number of alignment instructions.
        KBUILD_CFLAGS += $(call cc-option,$(cc_stack_align8))

	# Use -mskip-rax-setup if supported.
	KBUILD_CFLAGS += $(call cc-option,-mskip-rax-setup)

        # FIXME - should be integrated in Makefile.cpu (Makefile_32.cpu)
        cflags-$(CONFIG_MK8) += $(call cc-option,-march=k8)
        cflags-$(CONFIG_MPSC) += $(call cc-option,-march=nocona)

        cflags-$(CONFIG_MCORE2) += \
                $(call cc-option,-march=core2,$(call cc-option,-mtune=generic))
	cflags-$(CONFIG_MATOM) += $(call cc-option,-march=atom) \
		$(call cc-option,-mtune=atom,$(call cc-option,-mtune=generic))
        cflags-$(CONFIG_GENERIC_CPU) += $(call cc-option,-mtune=generic)
        KBUILD_CFLAGS += $(cflags-y)

        KBUILD_CFLAGS += -mno-red-zone
        KBUILD_CFLAGS += -mcmodel=kernel

        # -funit-at-a-time shrinks the kernel .text considerably
        # unfortunately it makes reading oopses harder.
        KBUILD_CFLAGS += $(call cc-option,-funit-at-a-time)
endif

ifdef CONFIG_X86_X32
	x32_ld_ok := $(call try-run,\
			/bin/echo -e '1: .quad 1b' | \
			$(CC) $(KBUILD_AFLAGS) -c -x assembler -o "$$TMP" - && \
			$(OBJCOPY) -O elf32-x86-64 "$$TMP" "$$TMPO" && \
			$(LD) -m elf32_x86_64 "$$TMPO" -o "$$TMP",y,n)
        ifeq ($(x32_ld_ok),y)
                CONFIG_X86_X32_ABI := y
                KBUILD_AFLAGS += -DCONFIG_X86_X32_ABI
                KBUILD_CFLAGS += -DCONFIG_X86_X32_ABI
        else
                $(warning CONFIG_X86_X32 enabled but no binutils support)
        endif
endif
export CONFIG_X86_X32_ABI

ifneq ($(CONFIG_X86),)
#
# If the function graph tracer is used with mcount instead of fentry,
# '-maccumulate-outgoing-args' is needed to prevent a GCC bug
# (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=42109)
#
ifdef CONFIG_FUNCTION_GRAPH_TRACER
  ifndef CONFIG_HAVE_FENTRY
	ACCUMULATE_OUTGOING_ARGS := 1
  else
    ifeq ($(call cc-option-yn, -mfentry), n)
	ACCUMULATE_OUTGOING_ARGS := 1

	# GCC ignores '-maccumulate-outgoing-args' when used with '-Os'.
	# If '-Os' is enabled, disable it and print a warning.
        ifdef CONFIG_CC_OPTIMIZE_FOR_SIZE
          undefine CONFIG_CC_OPTIMIZE_FOR_SIZE
          $(warning Disabling CONFIG_CC_OPTIMIZE_FOR_SIZE.  Your compiler does not have -mfentry so you cannot optimize for size with CONFIG_FUNCTION_GRAPH_TRACER.)
        endif

    endif
  endif
endif

ifndef CC_HAVE_ASM_GOTO
  $(error Compiler lacks asm-goto support.)
endif

#
# Jump labels need '-maccumulate-outgoing-args' for gcc < 4.5.2 to prevent a
# GCC bug (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=46226).  There's no way
# to test for this bug at compile-time because the test case needs to execute,
# which is a no-go for cross compilers.  So check the GCC version instead.
#
ifdef CONFIG_JUMP_LABEL
  ifneq ($(ACCUMULATE_OUTGOING_ARGS), 1)
	ACCUMULATE_OUTGOING_ARGS = $(call cc-if-fullversion, -lt, 040502, 1)
  endif
endif

ifeq ($(ACCUMULATE_OUTGOING_ARGS), 1)
	# This compiler flag is not supported by Clang:
	KBUILD_CFLAGS += $(call cc-option,-maccumulate-outgoing-args,)
endif
endif

# Stackpointer is addressed different for 32 bit and 64 bit x86
sp-$(CONFIG_X86_32) := esp
sp-$(CONFIG_X86_64) := rsp

# do binutils support CFI?
cfi := $(call as-instr,.cfi_startproc\n.cfi_rel_offset $(sp-y)$(comma)0\n.cfi_endproc,-DCONFIG_AS_CFI=1)
# is .cfi_signal_frame supported too?
cfi-sigframe := $(call as-instr,.cfi_startproc\n.cfi_signal_frame\n.cfi_endproc,-DCONFIG_AS_CFI_SIGNAL_FRAME=1)
cfi-sections := $(call as-instr,.cfi_sections .debug_frame,-DCONFIG_AS_CFI_SECTIONS=1)

# does binutils support specific instructions?
asinstr := $(call as-instr,fxsaveq (%rax),-DCONFIG_AS_FXSAVEQ=1)
asinstr += $(call as-instr,pshufb %xmm0$(comma)%xmm0,-DCONFIG_AS_SSSE3=1)
asinstr += $(call as-instr,crc32l %eax$(comma)%eax,-DCONFIG_AS_CRC32=1)
avx_instr := $(call as-instr,vxorps %ymm0$(comma)%ymm1$(comma)%ymm2,-DCONFIG_AS_AVX=1)
avx2_instr :=$(call as-instr,vpbroadcastb %xmm0$(comma)%ymm1,-DCONFIG_AS_AVX2=1)
avx512_instr :=$(call as-instr,vpmovm2b %k1$(comma)%zmm5,-DCONFIG_AS_AVX512=1)
sha1_ni_instr :=$(call as-instr,sha1msg1 %xmm0$(comma)%xmm1,-DCONFIG_AS_SHA1_NI=1)
sha256_ni_instr :=$(call as-instr,sha256msg1 %xmm0$(comma)%xmm1,-DCONFIG_AS_SHA256_NI=1)

KBUILD_AFLAGS += $(cfi) $(cfi-sigframe) $(cfi-sections) $(asinstr) $(avx_instr) $(avx2_instr) $(avx512_instr) $(sha1_ni_instr) $(sha256_ni_instr)
KBUILD_CFLAGS += $(cfi) $(cfi-sigframe) $(cfi-sections) $(asinstr) $(avx_instr) $(avx2_instr) $(avx512_instr) $(sha1_ni_instr) $(sha256_ni_instr)

LDFLAGS := -m elf_$(UTS_MACHINE)

#
# The 64-bit kernel must be aligned to 2MB.  Pass -z max-page-size=0x200000 to
# the linker to force 2MB page size regardless of the default page size used
# by the linker.
#
ifdef CONFIG_X86_64
LDFLAGS += $(call ld-option, -z max-page-size=0x200000)
endif

# Speed up the build
KBUILD_CFLAGS += -pipe
# Workaround for a gcc prelease that unfortunately was shipped in a suse release
KBUILD_CFLAGS += -Wno-sign-compare
#
KBUILD_CFLAGS += -fno-asynchronous-unwind-tables

# Avoid indirect branches in kernel to deal with Spectre
ifdef CONFIG_RETPOLINE
ifneq ($(RETPOLINE_CFLAGS),)
  KBUILD_CFLAGS += $(RETPOLINE_CFLAGS) -DRETPOLINE
endif
endif
endif

ifeq ($(L4X_SRCARCH),arm)
ifeq ($(CONFIG_CPU_ENDIAN_BE8),y)
LDFLAGS_vmlinux		+= --be8
KBUILD_LDFLAGS_MODULE	+= --be8
endif

ifeq ($(CONFIG_ARM_MODULE_PLTS),y)
KBUILD_LDFLAGS_MODULE	+= -T $(srctree)/arch/arm/kernel/module.lds
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
KBUILD_CPPFLAGS	+= -mbig-endian
CHECKFLAGS	+= -D__ARMEB__
AS		+= -EB
LD		+= -EB
else
KBUILD_CPPFLAGS	+= -mlittle-endian
CHECKFLAGS	+= -D__ARMEL__
AS		+= -EL
LD		+= -EL
endif

#
# The Scalar Replacement of Aggregates (SRA) optimization pass in GCC 4.9 and
# later may result in code being generated that handles signed short and signed
# char struct members incorrectly. So disable it.
# (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65932)
#
KBUILD_CFLAGS	+= $(call cc-option,-fno-ipa-sra)

# This selects which instruction set is used.
# Note that GCC does not numerically define an architecture version
# macro, but instead defines a whole series of macros which makes
# testing for a specific architecture or later rather impossible.
arch-$(CONFIG_CPU_32v7M)	=-D__LINUX_ARM_ARCH__=7 -march=armv7-m -Wa,-march=armv7-m
arch-$(CONFIG_CPU_32v7)		=-D__LINUX_ARM_ARCH__=7 $(call cc-option,-march=armv7-a,-march=armv5t -Wa$(comma)-march=armv7-a)
arch-$(CONFIG_CPU_32v6)		=-D__LINUX_ARM_ARCH__=6 $(call cc-option,-march=armv6,-march=armv5t -Wa$(comma)-march=armv6)
# Only override the compiler option if ARMv6. The ARMv6K extensions are
# always available in ARMv7
ifeq ($(CONFIG_CPU_32v6),y)
arch-$(CONFIG_CPU_32v6K)	=-D__LINUX_ARM_ARCH__=6 $(call cc-option,-march=armv6k,-march=armv5t -Wa$(comma)-march=armv6k)
endif
arch-$(CONFIG_CPU_32v5)		=-D__LINUX_ARM_ARCH__=5 $(call cc-option,-march=armv5te,-march=armv4t)
arch-$(CONFIG_CPU_32v4T)	=-D__LINUX_ARM_ARCH__=4 -march=armv4t
arch-$(CONFIG_CPU_32v4)		=-D__LINUX_ARM_ARCH__=4 -march=armv4
arch-$(CONFIG_CPU_32v3)		=-D__LINUX_ARM_ARCH__=3 -march=armv3

# Evaluate arch cc-option calls now
arch-y := $(arch-y)

# This selects how we optimise for the processor.
tune-$(CONFIG_CPU_ARM7TDMI)	=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM720T)	=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM740T)	=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM9TDMI)	=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM940T)	=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM946E)	=$(call cc-option,-mtune=arm9e,-mtune=arm9tdmi)
tune-$(CONFIG_CPU_ARM920T)	=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM922T)	=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM925T)	=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM926T)	=-mtune=arm9tdmi
tune-$(CONFIG_CPU_FA526)	=-mtune=arm9tdmi
tune-$(CONFIG_CPU_SA110)	=-mtune=strongarm110
tune-$(CONFIG_CPU_SA1100)	=-mtune=strongarm1100
tune-$(CONFIG_CPU_XSCALE)	=$(call cc-option,-mtune=xscale,-mtune=strongarm110) -Wa,-mcpu=xscale
tune-$(CONFIG_CPU_XSC3)		=$(call cc-option,-mtune=xscale,-mtune=strongarm110) -Wa,-mcpu=xscale
tune-$(CONFIG_CPU_FEROCEON)	=$(call cc-option,-mtune=marvell-f,-mtune=xscale)
tune-$(CONFIG_CPU_V6)		=$(call cc-option,-mtune=arm1136j-s,-mtune=strongarm)
tune-$(CONFIG_CPU_V6K)		=$(call cc-option,-mtune=arm1136j-s,-mtune=strongarm)

# Evaluate tune cc-option calls now
tune-y := $(tune-y)

ifeq ($(CONFIG_AEABI),y)
CFLAGS_ABI	:=-mabi=aapcs-linux -mno-thumb-interwork -mfpu=vfp
else
CFLAGS_ABI	:=$(call cc-option,-mapcs-32,-mabi=apcs-gnu) $(call cc-option,-mno-thumb-interwork,)
endif

ifeq ($(CONFIG_ARM_UNWIND),y)
CFLAGS_ABI	+=-funwind-tables
endif

# Accept old syntax despite ".syntax unified"
AFLAGS_NOWARN	:=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)

ifeq ($(CONFIG_THUMB2_KERNEL),y)
AFLAGS_AUTOIT	:=$(call as-option,-Wa$(comma)-mimplicit-it=always,-Wa$(comma)-mauto-it)
CFLAGS_ISA	:=-mthumb $(AFLAGS_AUTOIT) $(AFLAGS_NOWARN)
AFLAGS_ISA	:=$(CFLAGS_ISA) -Wa$(comma)-mthumb
# Work around buggy relocation from gas if requested:
ifeq ($(CONFIG_THUMB2_AVOID_R_ARM_THM_JUMP11),y)
KBUILD_CFLAGS_MODULE	+=-fno-optimize-sibling-calls
endif
else
CFLAGS_ISA	:=$(call cc-option,-marm,) $(AFLAGS_NOWARN)
AFLAGS_ISA	:=$(CFLAGS_ISA)
endif

# Need -Uarm for gcc < 3.x
KBUILD_CFLAGS	+=$(CFLAGS_ABI) $(CFLAGS_ISA) $(arch-y) $(tune-y) $(call cc-option,-mshort-load-bytes,$(call cc-option,-malignment-traps,)) -msoft-float -Uarm
KBUILD_AFLAGS	+=$(CFLAGS_ABI) $(AFLAGS_ISA) $(arch-y) $(tune-y) -include asm/unified.h -msoft-float

# Never generate .eh_frame
KBUILD_CFLAGS   += $(call cc-option,-fno-dwarf2-cfi-asm)

ifeq ($(CONFIG_FRAME_POINTER),y)
KBUILD_CFLAGS	+=-fno-omit-frame-pointer -mapcs -mno-sched-prolog
endif

endif


# -----------------------------------------------

-include $(objtree)/Makeconf.l4conf

L4OBJ := $(patsubst "%",%,$(CONFIG_L4_OBJ_TREE))
L4DIR := $(shell readlink $(L4OBJ)/source)
export L4OBJ
export L4DIR

ifneq ($(config-targets),1)
# Extra Libs
L4MODS-$(CONFIG_L4_VCPU)         += libvcpu
L4MODS-$(CONFIG_L4_USE_L4SHMC)   += shmc
L4MODS-$(CONFIG_L4_BLK_DRV)      += libfdx
L4MODS-$(CONFIG_L4_EXTERNAL_RTC) += rtc
L4MODS-$(CONFIG_L4_NET_ANKH_DRV) += ankh
L4MODS-$(CONFIG_L4_TCG_STPM)     += stpm-client
endif

L4INC		= -I$(L4OBJ)/include/$(L4_MK_ARCH)/$(L4_MK_API) \
		  -I$(L4OBJ)/include/$(L4_MK_ARCH) \
		  -I$(L4OBJ)/include/$(L4_MK_API) \
		  -I$(L4OBJ)/include

head-y := arch/l4/kernel/head.o

libs-y				+= arch/l4/lib/arch-$(L4X_SRCARCH)/	\
				   arch/l4/l4lxlib/generic/		\
				   arch/l4/l4lxlib/$(BASEENV)/
libs-$(CONFIG_L4_SERVER)	+= arch/l4/server/

drivers-y			+= arch/l4/drivers/
drivers-$(CONFIG_PCI)		+= arch/l4/pci/

ifeq ($(L4X_SRCARCH),x86)
drivers-$(CONFIG_PM)		+= arch/l4/power/arch-x86/
drivers-$(CONFIG_FB)		+= arch/x86/video/
core-$(CONFIG_KVM)		+= arch/l4/kvm/arch-x86/
endif

ifeq ($(L4X_SRCARCH),arm)
core-$(CONFIG_VDSO)		+= arch/arm/vdso/
core-y				+= arch/l4/common/arch-arm/
core-y				+= arch/l4/arm-mach-l4/
core-y				+= arch/arm/net/
endif

core-y += arch/l4/

-include $(L4OBJ)/l4defs.mk.inc

L4_REQUIRED_MODS		:= libc_be_minimal_log_io \
                                   libc_minimal libc_minimal_l4re \
				   libsupc++_minimal log l4re_c-util \
                                   $(if $(CONFIG_L4_LINK_LIBIO_DIRECT),libio-direct,libio) \
				   $(L4MODS-y)

L4_EXT_CFLAGS			:= $(call L4_BID_PKG_CONFIG_CALL,$(L4OBJ),--cflags --define-variable=libc_variant=libc,$(L4_REQUIRED_MODS))
ifneq ($(call L4_BID_PKG_CONFIG_FAILED,$(L4_EXT_CFLAGS)),)
$(info  Getting required compilation flags failed.)
$(info  L4OBJ: $(L4OBJ))
$(info  L4_REQUIRED_MODS: $(L4_REQUIRED_MODS))
$(error Aborting.)
endif

libs-y += $(L4OBJ)/lib/$(L4_SYSTEM)/l4f/libl4sys.a \
          $(L4OBJ)/lib/$(L4_SYSTEM)/l4f/libl4sys-direct.a \
          $(L4OBJ)/lib/$(L4_SYSTEM)/l4f/libgcc-l4.a

export L4INC L4_REQUIRED_MODS

LDFLAGS_vmlinux  =  \
  --defsym __L4_KIP_ADDR__=$(L4_BID_KIP_ADDR) \
  --defsym __l4sys_invoke_direct=$(L4_BID_KIP_ADDR)+$(L4_BID_KIP_OFFS_SYS_INVOKE) \
  --defsym __l4sys_debugger_direct=$(L4_BID_KIP_ADDR)+$(L4_BID_KIP_OFFS_SYS_DEBUGGER)

ifeq ($(CONFIG_HAVE_FUNCTION_TRACER),y)
KBUILD_CPPFLAGS += -I$(objtree)/arch/$(LINSRCARCH)/include/asm/trace
endif

# mix-in some more include paths
LINUXINCLUDE := $(subst -I$(srctree)/arch/l4/include, \
                        -I$(srctree)/arch/l4/include \
			  -I$(objtree)/arch/l4/include/asm/l4-arch \
                          -I$(srctree)/arch/$(LINSRCARCH)/include, \
			$(LINUXINCLUDE))
                        
LINUXINCLUDE := $(subst -I$(srctree)/arch/$(SRCARCH)/include/uapi, \
                        -I$(srctree)/arch/$(LINSRCARCH)/include/uapi, \
                        $(LINUXINCLUDE))

KBUILD_CPPFLAGS += -I$(objtree)/arch/$(LINSRCARCH)/include/generated \
                   -I$(objtree)/arch/$(LINSRCARCH)/include/generated/uapi \
                   $(if $(PLATFORMNAME),-I$(src)/arch/l4/include/asm/mach-$(LINSRCARCH)/$(PLATFORMNAME)) \
                   $(if $(PLATFORMNAME),-I$(src)/arch/l4/include/asm/plat-$(LINSRCARCH)/$(PLATFORMNAME)) \
                   $(if $(CONFIG_L4_ARCH_ARM),-I$(srctree)/arch/l4/arm-mach-l4/include) \
                   -I$(objtree)/arch/l4/include/asm/orig-arch \
                   -I$(objtree)/arch/l4/include \
                   $(L4INC) $(L4_EXT_CFLAGS) \
                   $(CPPFLAGS_SYSENTER-y) \
                   $(L4BID_CPPFLAGS_SYSCALLS-y)

# for DICE
KBUILD_CPPFLAGS	+= -DL4API_$(L4_MK_API)

KBUILD_CPPFLAGS += -DL4SYS_USE_UTCB_WRAP=1

ifneq ($(CONFIG_L4_ARCH_ARM),)
KBUILD_CPPFLAGS += -DTEXT_OFFSET=0x3000000
endif

# Build the DT binary blobs if we have OF configured
ifeq ($(CONFIG_USE_OF),y)
KBUILD_DTBS := dtbs
endif

all: lImage $(KBUILD_DTBS)

boot := arch/l4/boot

lImage: check_for_l4defs vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $@

check_for_l4defs:
	$(Q)if [ ! -e $(L4OBJ)/l4defs.mk.inc ]; then \
	  echo "No $(L4OBJ)/l4defs.mk.inc available."; \
	  echo "Did you run 'make' in $(L4OBJ) to a successful end?"; \
	  exit 1; \
	fi

PHONY += check_for_l4defs

mtags:
	(cd $(srctree) && $(MAKE) tags > /dev/null 2>&1; \
	sort tags > .tags.sorting && mv .tags.sorting tags )

arch/l4/include/asm/l4-arch/asm:
	@echo '  LN      asm-l4/l4-arch/asm symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(KBUILD_SRC),../../../../../source/arch/l4/include/asm,..)/arch-$(L4X_SRCARCH) $@

arch/l4/include/asm/arch:
	@echo '  LN      asm-l4/arch-$(L4X_SRCARCH)/arch -> asm/arch symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(KBUILD_SRC),../../../../source/arch/l4/include/asm/)arch-$(L4X_SRCARCH)/arch $@

arch/l4/include/asm/api:
	@echo '  LN      asm-l4/api-$(BASEENV) -> asm/api symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(KBUILD_SRC),../../../../source/arch/l4/include/asm/)api-$(BASEENV) $@

arch/l4/include/asm/l4x:
	@echo '  LN      asm-l4/l4x-$(L4X_SRCARCH) -> asm/l4x symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(KBUILD_SRC),../../../../source/arch/l4/include/asm/)l4x-$(L4X_SRCARCH) $@

arch/l4/include/asm/l4lxapi/impl:
	@echo '  LN      asm-l4/l4lxapi/impl symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(KBUILD_SRC),../../../../../source/arch/l4,../../..)/l4lxlib/l4env $@

arch/l4/include/asm/orig-arch/asm-$(LINSRCARCH):
	@echo '  LN      asm-l4/orig-arch symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(KBUILD_SRC),../../../../../source/arch,../../../..)/$(LINSRCARCH)/include/asm $@

symlink_files      = arch/l4/include/asm/api \
                     arch/l4/include/asm/l4lxapi/impl \
		     arch/l4/include/asm/l4x \
		     arch/l4/include/asm/l4-arch/asm

symlink_dirs       = arch/l4/include/asm/orig-arch/asm-$(LINSRCARCH)

MRPROPER_FILES	  += $(symlink_files) \
		     $(wildcard vmlinuz*) $(wildcard l4linux*) ldrImage source
MRPROPER_DIRS     += $(symlink_dirs)
CLEAN_FILES	  += Makeconf.l4conf

ifeq ($(config-targets),0)
ifneq ($(wildcard .config),)
$(objtree)/Makeconf.l4conf: $(srctree)/arch/l4/Makefile.l4conf $(srctree)/arch/l4/Makefile
	$(Q)$(if $(L4DIR),PWD=$(L4DIR),) $(MAKE) $(if $(L4DIR),-C $(L4DIR),) -f $(abspath $<) O=$(L4OBJ) OUTPUT=$(abspath $@) LINUX_SRCTREE=$(abspath $(srctree)) LINUX_OBJTREE=$(abspath $(objtree))

archprepare: print_instructions check_linux_config $(symlink_files) $(symlink_dirs) \
             $(objtree)/Makeconf.l4conf
ifeq ($(L4X_SRCARCH),arm)
	$(Q)$(MAKE) $(build)=arch/arm/tools kapi
	$(Q)cp $(srctree)/arch/arm/tools/mach-types .tmp.mach-types.list
	$(Q)echo "l4	MACH_L4	L4	9999" >> .tmp.mach-types.list
	$(Q)$(AWK) -f $(srctree)/arch/arm/tools/gen-mach-types .tmp.mach-types.list > .tmp.mach-types.h || { rm -f $@; /bin/false; }
	$(Q)cmp -s .tmp.mach-types.h arch/l4/include/generated/asm/mach-types.h || mv .tmp.mach-types.h arch/l4/include/generated/asm/mach-types.h
	$(Q)rm -f .tmp.mach-types.list .tmp.mach-types.h
endif
	$(Q)mkdir -p arch/$(L4X_SRCARCH)/lib
ifeq ($(CONFIG_KEXEC_FILE),y)
	$(Q)$(MAKE) $(build)=arch/x86/purgatory arch/x86/purgatory/kexec-purgatory.c
endif

endif # .config available
else # no archprepare when configuring the kernel
archprepare:
endif # config-targets == 1

# ----

archclean:
	$(Q)rm -rf $(objtree)/arch/i386
	$(Q)rm -rf $(objtree)/arch/x86_64
	$(Q)$(MAKE) $(clean)=$(boot)
	$(Q)$(MAKE) $(clean)=arch/x86/tools
	$(Q)$(MAKE) $(clean)=arch/x86/purgatory

ifeq ($(L4X_SRCARCH),x86)
# Syscall table generation

archheaders:
	$(Q)$(MAKE) $(build)=arch/x86/entry/syscalls all
endif

ifeq ($(L4X_SRCARCH),arm)
archheaders:
	$(Q)$(MAKE) $(build)=arch/arm/tools uapi
endif

chkl4conf:
	@if [ -d $(L4OBJ)/pkg/l4re-core/l4sys ]; then                        \
		echo "Configuration looks Ok.";                              \
	else                                                                 \
		echo \"$(L4OBJ)\" does not look like an L4 build directory!; \
	fi

## Check a few options that should not be enabled
# regparm: gcc may call memset from our libs, so obey calling convention
#          this should probably be fixed

# M486 M586 M586TSC M586MMX: cmpxchg64 emulation uses cli

DIS_OPTS-x86-32 := HPET_TIMER X86_X32
DIS_OPTS-x86-64 := PAGE_TABLE_ISOLATION
DIS_OPTS-arm-32 := PERF_EVENTS ARM_PATCH_PHYS_VIRT HIGHMEM
DIS_OPTS := HIGHMEM4G HIGHMEM64G PCI_GOANY PCI_GOBIOS \
            X86_MCE X86_MCE MTRR PARAVIRT KEXEC MCA SOFTWARE_SUSPEND APM \
	    VM86 X86_DS X86_PTRACE_BTS HT_IRQ DMI \
	    TRANSPARENT_HUGEPAGE COMPACTION X86_PAE ACPI X86_SMAP \
	    M486 M586 M586TSC M586MMX PNP \
	    X86_16BIT MICROCODE \
	    $(DIS_OPTS-$(L4X_SRCARCH)-$(BITS))
ENA_OPTS := $(ENA_OPTS-$(L4X_SRCARCH))

print_instructions:
	@if [ ! -e vmlinuz$(VMLINUZ_SUFFIX) ]; then \
	  echo ===============================================; \
	  echo "  For quick build instructions, please visit:"; \
	  echo "    http://wiki.tudos.org/Quickstart"; \
	  echo "    http://l4linux.org/build.shtml"; \
	  echo ===============================================; \
	fi

check_linux_config:
	@$(foreach i,$(addprefix CONFIG_,$(DIS_OPTS)),     \
	  if [ "$($(i))" = "y" ]; then                     \
	    echo; echo "ERROR: $(i) must not be enabled."; \
	    failed=1;                                      \
	  fi;)                                             \
	  if [ -n "$$failed" ]; then                       \
	    echo; exit 1;                                  \
	  fi
	@$(foreach i,$(addprefix CONFIG_,$(ENA_OPTS)),     \
	  if [ "$($(i))" != "y" ]; then                    \
	    echo; echo "ERROR: $(i) must be enabled.";     \
	    failed=1;                                      \
	  fi;)                                             \
	  if [ -n "$$failed" ]; then                       \
	    echo; exit 1;                                  \
	  fi
	@if [ "$(CONFIG_X86_32)" = "y" -a "$(CONFIG_PAGE_OFFSET)" != "0" ]; then \
	  echo "CONFIG_PAGE_OFFSET must be 0."; \
	  exit 1; \
	fi

%.dtb: | scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts MACHINE=$(MACHINE) $(boot)/dts/$@

PHONY += dtbs dtbs_install

dtbs: prepare scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts

dtbs_install:
	$(Q)$(MAKE) $(dtbinst)=$(boot)/dts

PHONY += chkl4conf check_linux_config

define archhelp
  echo  '* vmlinuz	 - Compressed kernel image'
  echo  '  chkl4conf     - Check L4 specific configuration'
  echo  '* dtbs          - Build device tree blobs for enabled boards'
  echo  '  dtbs_install  - Install dtbs to $(INSTALL_DTBS_PATH)'
endef
