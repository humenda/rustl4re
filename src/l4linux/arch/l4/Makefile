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

CHECKFLAGS	:= -D__arm__

L4_MK_ARCH	:= arm
OBJCOPYARCH	:= elf32-littlearm
endif

ifeq ($(LINSRCARCH),arm64)
BITS		:= 64
L4API		:= f
L4X_SRCARCH	:= arm64
CHECKFLAGS      += -D__AARCH64EL__
CHECKFLAGS      += -D__aarch64__
L4_MK_ARCH      := arm64
OBJCOPYARCH     := elf64-littleaarch64
endif

ifeq ($(LINSRCARCH),x86)
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

KBUILD_CFLAGS   += -pipe -DARCH_$(L4_MK_ARCH)

ifeq ($(L4X_SRCARCH),x86)

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

# Intel CET isn't enabled in the kernel
KBUILD_CFLAGS += $(call cc-option,-fcf-protection=none)
endif # X86

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

        # CPU-specific tuning. Anything which can be shared with UML should go here.
        include arch/x86/Makefile_32.cpu
        KBUILD_CFLAGS += $(cflags-y)

        # temporary until string.h is fixed
        KBUILD_CFLAGS += -ffreestanding

	ifeq ($(CONFIG_STACKPROTECTOR),y)
		ifeq ($(CONFIG_SMP),y)
			KBUILD_CFLAGS += -mstack-protector-guard-reg=fs -mstack-protector-guard-symbol=__stack_chk_guard
		else
			KBUILD_CFLAGS += -mstack-protector-guard=global
		endif
	endif
else
        BITS := 64
        UTS_MACHINE := x86_64
        CHECKFLAGS += -D__x86_64__

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
endif

ifdef CONFIG_X86_X32
	x32_ld_ok := $(call try-run,\
			/bin/echo -e '1: .quad 1b' | \
			$(CC) $(KBUILD_AFLAGS) -c -x assembler -o "$$TMP" - && \
			$(OBJCOPY) -O elf32-x86-64 "$$TMP" "$$TMP.o" && \
			$(LD) -m elf32_x86_64 "$$TMP.o" -o "$$TMP",y,n)
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

ifeq ($(ACCUMULATE_OUTGOING_ARGS), 1)
	# This compiler flag is not supported by Clang:
	KBUILD_CFLAGS += $(call cc-option,-maccumulate-outgoing-args,)
endif
endif # X86

# Workaround for a gcc prelease that unfortunately was shipped in a suse release
KBUILD_CFLAGS += -Wno-sign-compare
#
KBUILD_CFLAGS += -fno-asynchronous-unwind-tables

# Avoid indirect branches in kernel to deal with Spectre
ifdef CONFIG_RETPOLINE
  KBUILD_CFLAGS += $(RETPOLINE_CFLAGS)
  # Additionally, avoid generating expensive indirect jumps which
  # are subject to retpolines for small number of switch cases.
  # clang turns off jump table generation by default when under
  # retpoline builds, however, gcc does not for x86. This has
  # only been fixed starting from gcc stable version 8.4.0 and
  # onwards, but not for older ones. See gcc bug #86952.
  ifndef CONFIG_CC_IS_CLANG
    KBUILD_CFLAGS += $(call cc-option,-fno-jump-tables)
  endif
endif

KBUILD_LDFLAGS += -m elf_$(UTS_MACHINE)

ifdef CONFIG_LTO_CLANG
ifeq ($(shell test $(CONFIG_LLD_VERSION) -lt 130000; echo $$?),0)
KBUILD_LDFLAGS	+= -plugin-opt=-stack-alignment=$(if $(CONFIG_X86_32),4,8)
endif
endif

ifdef CONFIG_X86_NEED_RELOCS
LDFLAGS_vmlinux := --emit-relocs --discard-none
else
LDFLAGS_vmlinux :=
endif

#
# The 64-bit kernel must be aligned to 2MB.  Pass -z max-page-size=0x200000 to
# the linker to force 2MB page size regardless of the default page size used
# by the linker.
#
ifdef CONFIG_X86_64
LDFLAGS_vmlinux += -z max-page-size=0x200000
endif
endif

ifeq ($(L4X_SRCARCH),arm)
ifeq ($(CONFIG_CPU_ENDIAN_BE8),y)
LDFLAGS_vmlinux		+= --be8
KBUILD_LDFLAGS_MODULE	+= --be8
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
KBUILD_CPPFLAGS	+= -mbig-endian
CHECKFLAGS	+= -D__ARMEB__
KBUILD_LDFLAGS	+= -EB
else
KBUILD_CPPFLAGS	+= -mlittle-endian
CHECKFLAGS	+= -D__ARMEL__
KBUILD_LDFLAGS	+= -EL
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
arch-$(CONFIG_CPU_32v3)		=-D__LINUX_ARM_ARCH__=3 -march=armv3m

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
CFLAGS_ABI	:=-mabi=aapcs-linux -mfpu=vfp
else
CFLAGS_ABI	:=$(call cc-option,-mapcs-32,-mabi=apcs-gnu) $(call cc-option,-mno-thumb-interwork,)
endif

ifeq ($(CONFIG_ARM_UNWIND),y)
CFLAGS_ABI	+=-funwind-tables
endif

ifeq ($(CONFIG_CC_IS_CLANG),y)
CFLAGS_ABI	+= -meabi gnu
endif

# Accept old syntax despite ".syntax unified"
AFLAGS_NOWARN	:=$(call as-option,-Wa$(comma)-mno-warn-deprecated,-Wa$(comma)-W)

ifeq ($(CONFIG_THUMB2_KERNEL),y)
CFLAGS_ISA	:=-mthumb -Wa,-mimplicit-it=always $(AFLAGS_NOWARN)
AFLAGS_ISA	:=$(CFLAGS_ISA) -Wa$(comma)-mthumb
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
KBUILD_CFLAGS	+=-fno-omit-frame-pointer
ifeq ($(CONFIG_CC_IS_GCC),y)
KBUILD_CFLAGS += -mapcs -mno-sched-prolog
endif
endif

ifeq ($(CONFIG_STACKPROTECTOR_PER_TASK),y)
prepare: stack_protector_prepare
stack_protector_prepare: prepare0
	$(eval KBUILD_CFLAGS += \
		-fplugin-arg-arm_ssp_per_task_plugin-tso=$(shell	\
			awk '{if ($$2 == "THREAD_SZ_ORDER") print $$3;}'\
				include/generated/asm-offsets.h)	\
		-fplugin-arg-arm_ssp_per_task_plugin-offset=$(shell	\
			awk '{if ($$2 == "TI_STACK_CANARY") print $$3;}'\
				include/generated/asm-offsets.h))
endif
endif

ifneq ($(CONFIG_L4_ARCH_ARM64),)

cc_has_k_constraint := $(call try-run,echo				\
	'int main(void) {						\
		asm volatile("and w0, w0, %w0" :: "K" (4294967295));	\
		return 0;						\
	}' | $(CC) -S -x c -o "$$TMP" -,,-DCONFIG_CC_HAS_K_CONSTRAINT=1)

ifeq ($(CONFIG_BROKEN_GAS_INST),y)
$(warning Detected assembler with broken .inst; disassembly will be unreliable)
endif

KBUILD_CFLAGS	+= -mgeneral-regs-only	\
		   $(compat_vdso) $(cc_has_k_constraint)
KBUILD_CFLAGS	+= $(call cc-disable-warning, psabi)
KBUILD_AFLAGS	+= $(compat_vdso)

KBUILD_CFLAGS	+= $(call cc-option,-mabi=lp64)
KBUILD_AFLAGS	+= $(call cc-option,-mabi=lp64)

# Avoid generating .eh_frame* sections.
KBUILD_CFLAGS	+= -fno-asynchronous-unwind-tables -fno-unwind-tables
KBUILD_AFLAGS	+= -fno-asynchronous-unwind-tables -fno-unwind-tables

ifeq ($(CONFIG_STACKPROTECTOR_PER_TASK),y)
prepare: stack_protector_prepare
stack_protector_prepare: prepare0
	$(eval KBUILD_CFLAGS += -mstack-protector-guard=sysreg		  \
				-mstack-protector-guard-reg=sp_el0	  \
				-mstack-protector-guard-offset=$(shell	  \
			awk '{if ($$2 == "TSK_STACK_CANARY") print $$3;}' \
					include/generated/asm-offsets.h))
endif

# Ensure that if the compiler supports branch protection we default it
# off, this will be overridden if we are using branch protection.
branch-prot-flags-y += $(call cc-option,-mbranch-protection=none)

ifeq ($(CONFIG_ARM64_PTR_AUTH),y)
branch-prot-flags-$(CONFIG_CC_HAS_SIGN_RETURN_ADDRESS) := -msign-return-address=all
# We enable additional protection for leaf functions as there is some
# narrow potential for ROP protection benefits and no substantial
# performance impact has been observed.
ifeq ($(CONFIG_ARM64_BTI_KERNEL),y)
branch-prot-flags-$(CONFIG_CC_HAS_BRANCH_PROT_PAC_RET_BTI) := -mbranch-protection=pac-ret+leaf+bti
else
branch-prot-flags-$(CONFIG_CC_HAS_BRANCH_PROT_PAC_RET) := -mbranch-protection=pac-ret+leaf
endif
# -march=armv8.3-a enables the non-nops instructions for PAC, to avoid the
# compiler to generate them and consequently to break the single image contract
# we pass it only to the assembler. This option is utilized only in case of non
# integrated assemblers.
ifeq ($(CONFIG_AS_HAS_PAC), y)
asm-arch := armv8.3-a
endif
endif

KBUILD_CFLAGS += $(branch-prot-flags-y)

ifeq ($(CONFIG_AS_HAS_ARMV8_4), y)
# make sure to pass the newest target architecture to -march.
asm-arch := armv8.4-a
endif

ifdef asm-arch
KBUILD_CFLAGS	+= -Wa,-march=$(asm-arch) \
		   -DARM64_ASM_ARCH='"$(asm-arch)"'
endif

ifeq ($(CONFIG_SHADOW_CALL_STACK), y)
KBUILD_CFLAGS	+= -ffixed-x18
endif

ifeq ($(CONFIG_CPU_BIG_ENDIAN), y)
KBUILD_CPPFLAGS	+= -mbig-endian
CHECKFLAGS	+= -D__AARCH64EB__
# Prefer the baremetal ELF build target, but not all toolchains include
# it so fall back to the standard linux version if needed.
KBUILD_LDFLAGS	+= -EB $(call ld-option, -maarch64elfb, -maarch64linuxb -z norelro)
UTS_MACHINE	:= aarch64_be
else
KBUILD_CPPFLAGS	+= -mlittle-endian
CHECKFLAGS	+= -D__AARCH64EL__
# Same as above, prefer ELF but fall back to linux target if needed.
KBUILD_LDFLAGS	+= -EL $(call ld-option, -maarch64elf, -maarch64linux -z norelro)
UTS_MACHINE	:= aarch64
endif

ifeq ($(CONFIG_LD_IS_LLD), y)
KBUILD_LDFLAGS	+= -z norelro
endif

CHECKFLAGS	+= -D__aarch64__

ifeq ($(CONFIG_DYNAMIC_FTRACE_WITH_REGS),y)
  KBUILD_CPPFLAGS += -DCC_USING_PATCHABLE_FUNCTION_ENTRY
  CC_FLAGS_FTRACE := -fpatchable-function-entry=2
endif

endif # CONFIG_L4_ARCH_ARM64

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
drivers-$(CONFIG_L4_SERVER)	+= arch/l4/server/

drivers-y			+= arch/l4/drivers/
drivers-$(CONFIG_PCI)		+= arch/l4/pci/

ifeq ($(L4X_SRCARCH),x86)
drivers-$(CONFIG_PM)		+= arch/l4/power/arch-x86/
drivers-$(CONFIG_FB)		+= arch/x86/video/
core-$(CONFIG_KVM)		+= arch/l4/kvm/arch-x86/
endif

ifeq ($(L4X_SRCARCH),arm)
core-y				+= arch/l4/common/arch-arm/
core-y				+= arch/l4/arm-mach-l4/
core-y				+= arch/arm/probes/
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

# mix-in some more include paths
LINUXINCLUDE := $(subst -I$(srctree)/arch/l4/include, \
                        -I$(srctree)/arch/l4/include \
			  -I$(objtree)/arch/l4/include/asm/l4-arch \
                          -I$(srctree)/arch/$(LINSRCARCH)/include, \
			$(LINUXINCLUDE))
                        
LINUXINCLUDE := $(subst -I$(srctree)/arch/$(SRCARCH)/include/uapi, \
                        -I$(srctree)/arch/$(LINSRCARCH)/include/uapi, \
                        $(LINUXINCLUDE))

KBUILD_CPPFLAGS += $(if $(PLATFORMNAME),-I$(srctree)/arch/l4/include/asm/mach-$(LINSRCARCH)/$(PLATFORMNAME)) \
                   $(if $(PLATFORMNAME),-I$(srctree)/arch/l4/include/asm/plat-$(LINSRCARCH)/$(PLATFORMNAME)) \
                   $(if $(CONFIG_L4_ARCH_ARM),-I$(srctree)/arch/l4/arm-mach-l4/include) \
                   -I$(objtree)/arch/l4/include/asm/orig-arch \
                   -I$(objtree)/arch/l4/include \
                   $(L4INC) $(L4_EXT_CFLAGS) \
                   $(CPPFLAGS_SYSENTER-y) \
                   $(L4BID_CPPFLAGS_SYSCALLS-y)

# for DICE
KBUILD_CPPFLAGS	+= -DL4API_$(L4_MK_API)

KBUILD_CPPFLAGS += -DL4SYS_USE_UTCB_WRAP=1

# avoid inclusion of static_assert from l4/sys/compiler.h
# and since L4Linux is C only...
KBUILD_CFLAGS += -D__GXX_EXPERIMENTAL_CXX0X__

ifneq ($(CONFIG_L4_ARCH_ARM),)
KBUILD_CPPFLAGS += -DTEXT_OFFSET=0x3000000
endif

all: lImage

boot := arch/l4/boot

lImage: check_for_l4defs vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

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
	$(Q)ln -s $(if $(building_out_of_srctree),../../../../../source/arch/l4/include/asm,..)/arch-$(L4X_SRCARCH) $@

arch/l4/include/asm/arch:
	@echo '  LN      asm-l4/arch-$(L4X_SRCARCH)/arch -> asm/arch symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(building_out_of_srctree),../../../../source/arch/l4/include/asm/)arch-$(L4X_SRCARCH)/arch $@

arch/l4/include/asm/api:
	@echo '  LN      asm-l4/api-$(BASEENV) -> asm/api symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(building_out_of_srctree),../../../../source/arch/l4/include/asm/)api-$(BASEENV) $@

arch/l4/include/asm/l4x:
	@echo '  LN      asm-l4/l4x-$(L4X_SRCARCH) -> asm/l4x symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(building_out_of_srctree),../../../../source/arch/l4/include/asm/)l4x-$(L4X_SRCARCH) $@

arch/l4/include/asm/l4lxapi/impl:
	@echo '  LN      asm-l4/l4lxapi/impl symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(building_out_of_srctree),../../../../../source/arch/l4,../../..)/l4lxlib/l4env $@

arch/l4/include/asm/orig-arch/asm-$(LINSRCARCH):
	@echo '  LN      asm-l4/orig-arch/asm-$(LINSRCARCH) symlink'
	$(Q)mkdir -p $(@D)
	$(Q)ln -s $(if $(building_out_of_srctree),../../../../../source/arch,../../../..)/$(LINSRCARCH)/include/asm $@

symlink_files      = arch/l4/include/asm/api \
                     arch/l4/include/asm/l4lxapi/impl \
		     arch/l4/include/asm/l4x \
		     arch/l4/include/asm/l4-arch/asm

symlink_dirs       = arch/l4/include/asm/orig-arch/asm-$(LINSRCARCH)

MRPROPER_FILES	  += $(symlink_files) \
		     $(wildcard vmlinuz*) $(wildcard l4linux*) ldrImage source
MRPROPER_DIRS     += $(symlink_dirs)
CLEAN_FILES	  += Makeconf.l4conf

$(objtree)/Makeconf.l4conf: $(srctree)/arch/l4/Makefile.l4conf $(srctree)/arch/l4/Makefile
	$(Q)$(if $(L4DIR),PWD=$(L4DIR),) $(MAKE) $(if $(L4DIR),-C $(L4DIR),) -f $(abspath $<) O=$(L4OBJ) OUTPUT=$(abspath $@) LINUX_SRCTREE=$(abspath $(srctree)) LINUX_OBJTREE=$(abspath $(objtree))

archprepare: checkbin print_instructions check_linux_config $(symlink_files) $(symlink_dirs) \
             $(objtree)/Makeconf.l4conf archprepare_l4x

archprepare_l4x:
ifeq ($(L4X_SRCARCH),arm)
	$(Q)$(MAKE) $(build)=arch/arm/tools kapi
	$(Q)cp $(srctree)/arch/arm/tools/mach-types .tmp.mach-types.list
	$(Q)echo "l4	MACH_L4	L4	9999" >> .tmp.mach-types.list
	$(Q)$(AWK) -f $(srctree)/arch/arm/tools/gen-mach-types .tmp.mach-types.list > .tmp.mach-types.h || { rm -f $@; /bin/false; }
	$(Q)cmp -s .tmp.mach-types.h arch/l4/include/generated/asm/mach-types.h || mv .tmp.mach-types.h arch/l4/include/generated/asm/mach-types.h
	$(Q)rm -f .tmp.mach-types.list .tmp.mach-types.h
endif
ifeq ($(L4X_SRCARCH),arm64)
	$(Q)$(MAKE) $(build)=arch/arm64/tools kapi
endif
	$(Q)mkdir -p arch/$(L4X_SRCARCH)/lib

checkbin:
ifndef CONFIG_CC_HAS_ASM_GOTO
	@echo Compiler lacks asm-goto support.
	@exit 1
endif
ifdef CONFIG_RETPOLINE
ifeq ($(RETPOLINE_CFLAGS),)
	@echo "You are building kernel with non-retpoline compiler." >&2
	@echo "Please update your compiler." >&2
	@false
endif
endif

# ----

archclean:
	$(Q)rm -rf $(objtree)/arch/i386
	$(Q)rm -rf $(objtree)/arch/x86_64
	$(Q)$(MAKE) $(clean)=$(boot)
	$(Q)$(MAKE) $(clean)=arch/x86/tools

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

DIS_OPTS-x86    := CPU_SUP_INTEL CPU_SUP_AMD CPU_SUP_HYGON \
                   CPU_SUP_CENTAUR CPU_SUP_ZHAOXIN
DIS_OPTS-x86-32 := HPET_TIMER X86_X32 $(DIS_OPTS-x86)
DIS_OPTS-x86-64 := PAGE_TABLE_ISOLATION $(DIS_OPTS-x86)
DIS_OPTS-arm-32 := PERF_EVENTS ARM_PATCH_PHYS_VIRT HIGHMEM \
                   HARDEN_BRANCH_PREDICTOR
DIS_OPTS-arm-64 := ARM64_PTR_AUTH
DIS_OPTS := HIGHMEM4G HIGHMEM64G PCI_GOANY PCI_GOBIOS \
            X86_MCE MTRR PARAVIRT KEXEC MCA SOFTWARE_SUSPEND APM \
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

PHONY += chkl4conf check_linux_config

ifneq ($(CONFIG_L4_ARCH_ARM64),)

PHONY += vdso_install
vdso_install:
	$(Q)$(MAKE) $(build)=arch/arm64/kernel/vdso $@
	$(if $(CONFIG_COMPAT_VDSO), \
		$(Q)$(MAKE) $(build)=arch/arm64/kernel/vdso32 $@)

ifeq ($(KBUILD_EXTMOD),)
# We need to generate vdso-offsets.h before compiling certain files in kernel/.
# In order to do that, we should use the archprepare target, but we can't since
# asm-offsets.h is included in some files used to generate vdso-offsets.h, and
# asm-offsets.h is built in prepare0, for which archprepare is a dependency.
# Therefore we need to generate the header after prepare0 has been made, hence
# this hack.
prepare: vdso_prepare
vdso_prepare: prepare0
	$(Q)$(MAKE) $(build)=arch/arm64/kernel/vdso \
	include/generated/vdso-offsets.h arch/arm64/kernel/vdso/vdso.so
ifdef CONFIG_COMPAT_VDSO
	$(Q)$(MAKE) $(build)=arch/arm64/kernel/vdso32 \
	include/generated/vdso32-offsets.h arch/arm64/kernel/vdso32/vdso.so
endif
endif
endif # ARM64


define archhelp
  echo  '* vmlinuz	 - Compressed kernel image'
  echo  '  chkl4conf     - Check L4 specific configuration'
endef
