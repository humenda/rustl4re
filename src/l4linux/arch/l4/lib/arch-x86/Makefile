# SPDX-License-Identifier: GPL-2.0
#
# Makefile for x86 specific library files.
#

b := ../../../x86/lib

# Produces uninteresting flaky coverage.
KCOV_INSTRUMENT_delay.o	:= n

# KCSAN uses udelay for introducing watchpoint delay; avoid recursion.
KCSAN_SANITIZE_delay.o := n
ifdef CONFIG_KCSAN
# In case KCSAN+lockdep+ftrace are enabled, disable ftrace for delay.o to avoid
# lockdep -> [other libs] -> KCSAN -> udelay -> ftrace -> lockdep recursion.
CFLAGS_REMOVE_delay.o = $(CC_FLAGS_FTRACE)
endif

# Early boot use of cmdline; don't instrument it
ifdef CONFIG_AMD_MEM_ENCRYPT
KCOV_INSTRUMENT_cmdline.o := n
KASAN_SANITIZE_cmdline.o  := n
KCSAN_SANITIZE_cmdline.o  := n

ifdef CONFIG_FUNCTION_TRACER
CFLAGS_REMOVE_cmdline.o = -pg
endif

CFLAGS_cmdline.o := -fno-stack-protector -fno-jump-tables
endif

inat_tables_script = $(srctree)/arch/x86/tools/gen-insn-attr-x86.awk
inat_tables_maps = $(srctree)/arch/x86/lib/x86-opcode-map.txt
quiet_cmd_inat_tables = GEN     $@
      cmd_inat_tables = $(AWK) -f $(inat_tables_script) $(inat_tables_maps) > $@

$(obj)/inat-tables.c: $(inat_tables_script) $(inat_tables_maps)
	$(call cmd,inat_tables)

$(obj)/$(b)/inat.o: $(obj)/inat-tables.c

clean-files := inat-tables.c

obj-$(CONFIG_SMP) += msr-smp.o cache-smp.o

KBUILD_CFLAGS += -Iarch/x86/lib
KBUILD_CFLAGS += -Iarch/l4/lib/arch-x86

lib-y := delay.o misc.o cmdline.o cpu.o
lib-y += usercopy_$(BITS).o usercopy.o
lib-y += memcpy_$(BITS).o
lib-$(CONFIG_ARCH_HAS_COPY_MC) += copy_mc.o copy_mc_64.o
lib-$(CONFIG_INSTRUCTION_DECODER) += insn.o inat.o insn-eval.o
lib-$(CONFIG_RANDOMIZE_BASE) += kaslr.o
lib-$(CONFIG_FUNCTION_ERROR_INJECTION)	+= error-inject.o
lib-$(CONFIG_RETPOLINE) += retpoline.o

obj-y += msr.o msr-reg.o msr-reg-export.o hweight.o
obj-y += iomem.o

ifeq ($(CONFIG_X86_32),y)
        obj-y += atomic64_32.o
        lib-y += atomic64_cx8_32.o
        lib-y += checksum_32.o
        lib-y += strstr_32.o
        lib-y += string_32.o
ifneq ($(CONFIG_X86_CMPXCHG64),y)
        lib-y += cmpxchg8b_emu.o atomic64_386_32.o
endif
        lib-$(CONFIG_X86_USE_3DNOW) += mmx_32.o
else
        obj-y += iomap_copy_64.o
        lib-y += csum-partial_64.o csum-copy_64.o csum-wrappers_64.o
        lib-y += clear_page_64.o copy_page_64.o
        lib-y += memmove_64.o memset_64.o
        #lib-y += copy_user_64.o
	lib-y += cmpxchg16b_emu.o
endif

lib-y                  += ../memcpy_user.o ../uaccess.o ../pte.o

obj-y                  += ../proc.o
obj-$(CONFIG_L4_DEBUG) += ../debugfs.o

include arch/l4/Makefile.lib

$(call L4X_FILEREF, $(b),                                     \
       atomic64_32 cache-smp cmpxchg16b_emu hash hweight      \
       iomap_copy_64 misc msr msr-reg msr-reg-export msr-smp  \
       iomem)

lib-y := $(call L4X_PREFIX_IF, $(b), $(lib-y),                  \
                delay misc cmdline cpu memcpy_$(BITS) insn-eval \
	        usercopy rwsem insn inat atomic64_cx8_32        \
	        strstr_32 string_32 atomic64_386_32 mmx_32      \
	        csum-partial_64 csum-copy_64 clear_page_64      \
		copy_page_64 memmove_64 memset_64 cmpxchg16b_emu\
		retpoline error-inject)

CFLAGS_usercopy.o      += -Dstrncpy_from_user=strncpy_from_user_orig
