PKGDIR     ?= ../../..
L4DIR      ?= $(PKGDIR)/../..
CONTRIB    ?= $(PKGDIR)/linux26/contrib

-include $(PKGDIR_OBJ)/Makeconf

ifeq ($(CONFIG_DDE26_NET),y)
TARGET += libdde_linux26_net_ip.a
endif

DDE_SYSTEMS = x86

ifeq ($(ARCH), x86)
ARCH_DIR = arch/i386
endif

# contrib sources are in $(CONTRIB)
vpath %.c     $(CONTRIB) $(PKGDIR)/linux26/lib/src
vpath %.S     $(CONTRIB) $(PKGDIR)/linux26/lib/src

PRIVATE_INCDIR += $(CONTRIB)/drivers/pci $(PKGDIR)/linux26/lib/src/arch/l4 \
                  $(CONTRIB)/$(ARCH_DIR)/pci $(CONTRIB)/drivers/base/ \
                  $(CONTRIB)/lib $(PKGDIR_OBJ) $(CONTRIB)/net/core

ifeq ($(ARCH), x86)
SRC_S_libdde_linux26_net_ip.a += $(ARCH_DIR)/lib/checksum.S
endif

SRC_C_libdde_linux26_net_ip.a += \
                              arch/l4/net.c \
                              drivers/net/mii.c \
                              net/core/skbuff.c \
                              net/core/utils.c \
                              net/core/dev.c \
                              net/core/ethtool.c \
                              net/core/link_watch.c \
                              net/core/dev_mcast.c \
                              net/core/neighbour.c \
                              net/core/netevent.c \
                              net/ethernet/eth.c \
                              net/sched/sch_generic.c \
                              arch/l4/inodes.c      \
                              mm/memory.c \
                              net/core/filter.c \
                              net/core/rtnetlink.c \
                              net/core/sock.c \
                              net/netlink/af_netlink.c \
                              net/netlink/attr.c

include $(PKGDIR)/linux26/Makeconf

include $(L4DIR)/mk/lib.mk
