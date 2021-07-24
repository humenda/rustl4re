#
# arch/l4/boot/Makefile
#

quiet_cmd_localinst = LOCINST $@
      cmd_localinst = ([ -x "$(srctree)/arch/l4/install.local" ] && \
                       $(srctree)/arch/l4/install.local $@ $(L4X_SRCARCH)) || true

OBJCOPYFLAGS_Image := -R .note -R .comment -S
$(obj)/Image: vmlinux FORCE
	$(call if_changed,objcopy)

$(obj)/lImage: $(obj)/Image vmlinuz$(VMLINUZ_SUFFIX) l4linux$(VMLINUZ_SUFFIX)

vmlinuz$(VMLINUZ_SUFFIX): ldrImage FORCE
	$(call if_changed,gzip)
	$(call cmd,localinst)

l4linux$(VMLINUZ_SUFFIX): ldrImage FORCE
	$(Q)ln -sf $< $@

objects-y := launch.o ldr.o res.o image.o

# maintenance targets such as clean do not read in the config,
# as $(dot-config) does not work from herein, we check L4OBJ
ifneq ($(L4OBJ),)
include $(L4OBJ)/l4defs.mk.inc

override CC = $(L4_CC)
override LD = $(L4_LD)
endif # non-empty $(L4OBJ)

ifeq ($(LD),)
  $(error Error: L4_LD variable empty. Rebuild L4 tree)
endif
ifeq ($(CC),)
  $(error Error: L4_CC variable empty. Rebuild L4 tree)
endif

# we compile an L4 program, so we use L4 options for the compiler
KBUILD_CFLAGS   := $(L4_CFLAGS_MINIMAL)

KBUILD_CFLAGS	+= $(L4INC) -I$(L4OBJ)/include/uclibc \
		   -Wall -fno-strict-aliasing -O2 -pipe \
		   -DARCH_$(L4_MK_ARCH) -g \
		   -I$(obj) -DL4SYS_USE_UTCB_WRAP=1 \
		   -Wno-unused-parameter -Wno-sign-compare -Wno-pointer-sign

ifdef CONFIG_RETPOLINE
ifneq ($(RETPOLINE_CFLAGS),)
  KBUILD_CFLAGS += $(RETPOLINE_CFLAGS) -DRETPOLINE
endif
endif

KBUILD_AFLAGS   += -DVMLINUX_IMAGE=\"$(obj)/Image\"

# Disable UBSAN for the loader
UBSAN_SANITIZE := n

$(obj)/res.o: $(obj)/func_list.h

$(obj)/image.o: $(obj)/Image

$(obj)/func_list.h: vmlinux
	$(Q)$(OBJCOPY) -j .data..l4externals.str -O binary $< $@.tmp
	$(Q)if [ ! -s $@.tmp ]; then echo "Section empty"; exit 1; fi
	$(Q)perl -p -e 's/(.+?)\0/EF($$1)\n/g' $@.tmp > $@
	$(Q)rm $@.tmp

OBJS := $(addprefix $(obj)/,$(objects-y))
ifdef CONFIG_X86
ifdef CONFIG_RETPOLINE
OBJS += arch/x86/lib/retpoline.o
endif
endif

L4_EXT_LIBS := $(call L4_BID_PKG_CONFIG_CALL,$(L4OBJ),--libs --define-variable=libc_variant=libc,$(L4_REQUIRED_MODS))

ifneq ($(call L4_BID_PKG_CONFIG_FAILED,$(L4_EXT_LIBS)),)
$(info  Getting required libraries failed.)
$(info  L4OBJ: $(L4OBJ))
$(info  L4_REQUIRED_MODS: $(L4_REQUIRED_MODS))
$(error Aborting.)
endif

L4LIBS := $(shell $(CC) $(KBUILD_CFLAGS) \
          -print-file-name=libgcc_eh.a) $(L4_EXT_LIBS)

servers-y := $(if $(CONFIG_L4_SERVER),arch/l4/server/lib.a)

LINKADDR-32 = 0xa8000000
LINKADDR-64 = 0x68000000

bid_link = $(L4DIR)/tool/bin/l4-bender -t ld \
           $(call L4_BID_BENDER_DEFINES,$(L4OBJ),$(L4DIR),$(L4_GCCSYSLIBDIRS),$(L4_SYSTEM),l4f) \
           --spec=$(L4DIR)/mk/bid-bender.spec -Dlinker="$(L4_LD)" --

ldrImage: $(OBJS)
	@echo "  LD      $@"
	$(Q)$(bid_link) $(LDFLAGS) -gc-sections -o $@ \
	  -static $(L4_LIBDIRS) $(OBJS) $(servers-y) \
	  -Ttext-segment=$(LINKADDR-$(BITS)) \
	  --defsym __L4_STACK_ADDR__=$(L4_BID_STACK_ADDR) \
	  --defsym __L4_KIP_ADDR__=$(L4_BID_KIP_ADDR) \
          --defsym __l4sys_invoke_direct=$(L4_BID_KIP_ADDR)+$(L4_BID_KIP_OFFS_SYS_INVOKE) \
          --defsym __l4sys_debugger_direct=$(L4_BID_KIP_ADDR)+$(L4_BID_KIP_OFFS_SYS_DEBUGGER) \
	  $(addprefix -PC,$(L4_REQUIRED_MODS))

PHONY += lImage
