# SPDX-License-Identifier: GPL-2.0
#
# arch/l4/kernel/arch-arm/Makefile
#

CPPFLAGS_vmlinux.lds := -DTEXT_OFFSET=0x0200000

ifdef CONFIG_FUNCTION_TRACER
CFLAGS_REMOVE_ftrace.o = -pg
CFLAGS_REMOVE_insn.o = -pg
CFLAGS_REMOVE_patch.o = -pg
endif

CFLAGS_REMOVE_return_address.o = -pg

# Object file lists.

obj-y		:= elf.o entry.o irq.o opcodes.o \
		   process.o ptrace.o reboot.o return_address.o \
		   setup.o signal.o sigreturn_codes.o \
		   stacktrace.o sys_arm.o time.o traps.o

obj-y		+= head.o dispatch.o switch_to.o

obj-$(CONFIG_ATAGS)		+= atags_parse.o
obj-$(CONFIG_ATAGS_PROC)	+= atags_proc.o
obj-$(CONFIG_DEPRECATED_PARAM_STRUCT) += atags_compat.o

ifeq ($(CONFIG_CPU_V7M),y)
#obj-y		+= entry-v7m.o v7m.o
else
#obj-y		+= entry-armv.o
endif

obj-$(CONFIG_CPU_IDLE)		+= cpuidle.o
obj-$(CONFIG_ISA_DMA_API)	+= dma.o
#obj-$(CONFIG_FIQ)		+= fiq.o fiqasm.o
obj-$(CONFIG_MODULES)		+= armksyms.o module.o
obj-$(CONFIG_ARM_MODULE_PLTS)	+= module-plts.o
#obj-$(CONFIG_ISA_DMA)		+= dma-isa.o
#obj-$(CONFIG_PCI)		+= bios32.o isa.o
#obj-$(CONFIG_ARM_CPU_SUSPEND)	+= sleep.o suspend.o
#obj-$(CONFIG_HIBERNATION)	+= hibernate.o
obj-$(CONFIG_SMP)		+= smp.o
ifdef CONFIG_MMU
obj-$(CONFIG_SMP)		+= smp_tlb.o
endif
#obj-$(CONFIG_HAVE_ARM_SCU)	+= smp_scu.o
#obj-$(CONFIG_HAVE_ARM_TWD)	+= smp_twd.o
obj-$(CONFIG_ARM_ARCH_TIMER)	+= arch_timer.o
obj-$(CONFIG_FUNCTION_TRACER)	+= entry-ftrace.o
obj-$(CONFIG_DYNAMIC_FTRACE)	+= ftrace.o insn.o
obj-$(CONFIG_FUNCTION_GRAPH_TRACER)	+= ftrace.o insn.o
#obj-$(CONFIG_JUMP_LABEL)	+= jump_label.o insn.o patch.o
#obj-$(CONFIG_KEXEC)		+= machine_kexec.o relocate_kernel.o
## Main staffs in KPROBES are in arch/arm/probes/ .
#obj-$(CONFIG_KPROBES)		+= patch.o insn.o
obj-$(CONFIG_OABI_COMPAT)	+= sys_oabi-compat.o
#obj-$(CONFIG_ARM_THUMBEE)	+= thumbee.o
obj-$(CONFIG_KGDB)		+= kgdb.o patch.o
obj-$(CONFIG_ARM_UNWIND)	+= unwind.o
#obj-$(CONFIG_HAVE_TCM)		+= tcm.o
obj-$(CONFIG_OF)		+= devtree.o
obj-$(CONFIG_CRASH_DUMP)	+= crash_dump.o
obj-$(CONFIG_SWP_EMULATE)	+= swp_emulate.o
CFLAGS_swp_emulate.o		:= -Wa,-march=armv7-a
#obj-$(CONFIG_HAVE_HW_BREAKPOINT)	+= hw_breakpoint.o

#obj-$(CONFIG_CPU_XSCALE)	+= xscale-cp0.o
#obj-$(CONFIG_CPU_XSC3)		+= xscale-cp0.o
#obj-$(CONFIG_CPU_MOHAWK)	+= xscale-cp0.o
#obj-$(CONFIG_CPU_PJ4)		+= pj4-cp0.o
#obj-$(CONFIG_CPU_PJ4B)		+= pj4-cp0.o
#obj-$(CONFIG_IWMMXT)		+= iwmmxt.o
obj-$(CONFIG_PERF_EVENTS)	+= perf_regs.o perf_callchain.o
obj-$(CONFIG_HW_PERF_EVENTS)	+= perf_event_xscale.o perf_event_v6.o \
				   perf_event_v7.o
AFLAGS_iwmmxt.o			:= -Wa,-mcpu=iwmmxt
obj-$(CONFIG_ARM_CPU_TOPOLOGY)  += topology.o
obj-$(CONFIG_VDSO)		+= vdso.o
obj-$(CONFIG_EFI)		+= efi.o

ifneq ($(CONFIG_ARCH_EBSA110),y)
  obj-y		+= io.o
endif
#obj-$(CONFIG_PARAVIRT)	+= paravirt.o

#head-y			:= head$(MMUEXT).o
obj-$(CONFIG_DEBUG_LL)	+= debug.o
#obj-$(CONFIG_EARLY_PRINTK)	+= early_printk.o

# This is executed very early using a temporary stack when no memory allocator
# nor global data is available. Everything has to be allocated on the stack.
#CFLAGS_head-inflate-data.o := $(call cc-option,-Wframe-larger-than=10240)
#obj-$(CONFIG_XIP_DEFLATED_DATA) += head-inflate-data.o

obj-$(CONFIG_ARM_VIRT_EXT)	+= hyp-stub.o
AFLAGS_hyp-stub.o		:=-Wa,-march=armv7-a
ifeq ($(CONFIG_ARM_PSCI),y)
#obj-$(CONFIG_SMP)		+= psci_smp.o
endif

#obj-$(CONFIG_HAVE_ARM_SMCCC)	+= smccc-call.o

obj-$(CONFIG_L4_VPCI)		+= bios32.o

include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../../arm/kernel,                  \
       atags_parse atags_proc compat cpuidle crash_dump   \
       devtree dma elf hyp-stub io irq kgdb module        \
       opcodes patch return_address sigreturn_codes       \
       stacktrace swp_emulate sys_oabi-compat time        \
       topology unwind entry-ftrace ftrace insn)

KBUILD_CPPFLAGS	+= -I$(srctree)/arch/arm/kernel

# ----------------------------------------------------------------------
# _upage_start must(!) be last so that ld gets its calculation right!
define get_upage_defsyms
	upage_base=0xbf008000;						\
	for i in $$($(NM) $< | cut -d\  -f3 | grep -vw l4x_upage_start | grep -ie '^[a-z]');	\
	do								\
		echo -defsym $$i=$$i-l4x_upage_start+$$upage_base;	\
	done;								\
	echo -defsym l4x_upage_start=$$upage_base;
endef

obj-y		+= upage.reloced.o
targets		+= upage.o

quiet_cmd_upage = UPAGE   $@
      cmd_upage = $(LD) $(EXTRA_LDFLAGS) -N \
                        $(shell $(get_upage_defsyms)) -T /dev/null -o $@ $<

$(obj)/upage.reloced.o: $(obj)/upage.o FORCE
	$(call if_changed,upage)
