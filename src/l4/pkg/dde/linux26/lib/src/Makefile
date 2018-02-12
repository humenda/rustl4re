PKGDIR     ?= ../../..
L4DIR      ?= $(PKGDIR)/../..
CONTRIB    ?= $(PKGDIR)/linux26/contrib

-include $(PKGDIR_OBJ)/Makeconf

# Building the main DDE library takes place incrementally. First, we build a temporary
# libdde_linux26_DONT_USE.o.a -- .o.a is used so that no functions are omitted by the
# linker. Otherwise it would remove certain Linux initcalls from the library which we
# rely on.
#
# After building the Linux support lib, we need to take care of Linux symbols that may
# also be defined in other libraries, such as LibC's str*() functions. To make sure that
# clients can use libdde along with those other libs, we use objcopy to make all symbols
# be defined weakly. Only those functions that are needed by drivers outside libdde_linux
# are explicitly made global. (To add a function to be exported, put it into one of the
# global-symbols.{all, arm, x86} files in the dde/linux26/lib/src directory.)

TARGET_DDE       = libdde-linux26.a
TARGET_DDE_NET   = libdde-linux26_net.a
TARGET_DDE_CHAR  = libdde-linux26_char.a

#ifeq ($(CONFIG_DDE26_BLOCK),y)
TARGET_DDE_BLOCK = libdde-linux26_block.a
TARGET_DDE_PCI = libdde-linux26_pci.a
#endif

ifeq ($(CONFIG_DDE26_SOUND),y)
TARGET_DDE_SOUND = libdde-linux26_sound.a
endif

TARGET_DDE_FS = libdde-linux26_fs.a

DDE_SYSTEMS = x86 arm

ifeq ($(ARCH), x86)
ARCH_DIR = arch/x86
endif

ifeq ($(ARCH), arm)
ARCH_DIR = arch/arm
MARCH = realview
DEFINES += -D__LINUX_ARM_ARCH__=6
endif

PC_FILENAME = dde-linux26
PC_FILENAMES                 := $(PC_FILENAME) $(PC_FILENAME)_net $(PC_FILENAME)_block $(PC_FILENAME)_char $(PC_FILENAME)_fs $(PC_FILENAME)_pci
PC_LIBS_$(PC_FILENAME)       := $(patsubst lib%.a,-l%,$(TARGET_DDE))
PC_LIBS_$(PC_FILENAME)_net   := $(patsubst lib%.a,-l%,$(TARGET_DDE_NET))
PC_LIBS_$(PC_FILENAME)_block := $(patsubst lib%.a,-l%,$(TARGET_DDE_BLOCK))
PC_LIBS_$(PC_FILENAME)_pci   := $(patsubst lib%.a,-l%,$(TARGET_DDE_PCI))
PC_LIBS_$(PC_FILENAME)_sound := $(patsubst lib%.a,-l%,$(TARGET_DDE_SOUND))
PC_LIBS_$(PC_FILENAME)_char  := $(patsubst lib%.a,-l%,$(TARGET_DDE_CHAR))
PC_LIBS_$(PC_FILENAME)_fs := $(patsubst lib%.a,-l%,$(TARGET_DDE_FS))
REQUIRES_LIBS := ddekit

TARGET     = $(TARGET_DDE) $(TARGET_DDE_NET) $(TARGET_DDE_BLOCK) $(TARGET_DDE_SOUND) $(TARGET_DDE_CHAR) $(TARGET_DDE_FS) $(TARGET_DDE_PCI)
LINK_INCR  = $(TARGET_DDE) $(TARGET_DDE_BLOCK)
LINK_INCR_ONLYGLOBSYMFILE_$(TARGET_DDE) = $(SRC_DIR)/global-symbols.all \
                                          $(SRC_DIR)/global-symbols.$(ARCH)

# contrib sources are in $(CONTRIB)
vpath %.c $(CONTRIB)
vpath %.S $(CONTRIB)

PRIVATE_INCDIR += $(CONTRIB)/drivers/pci $(PKGDIR)/linux26/lib/src/arch/l4 \
                  $(CONTRIB)/$(ARCH_DIR)/pci $(CONTRIB)/drivers/base/ \
                  $(CONTRIB)/lib $(PKGDIR_OBJ) $(CONTRIB)/net/core $(CONTRIB)/fs


##################################################################
# Sources for libdde_linux.a                                     #
##################################################################
SRC_DDE = cli_sti.c fs.c hw-helpers.c init_task.c init.c power.c \
          process.c res.c sched.c signal.c smp.c softirq.c timer.c \
          page_alloc.c kmem_cache.c kmalloc.c irq.c param.c \
          vmalloc.c vmstat.c mm-helper.c dummy.c

# our implementation
SRC_C_$(TARGET_DDE) = $(addprefix arch/l4/, $(SRC_DDE))

ifeq ($(ARCH), x86)
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/semaphore_32.S
SRC_C_$(TARGET_DDE) += lib/rwsem.c
SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/kernel/pci-dma.c
SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/kernel/pci-nommu.c
SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/lib/string_32.c
SRC_S_$(TARGET_DDE_NET) += $(ARCH_DIR)/lib/checksum_32.S
endif

ifeq ($(ARCH), arm)
SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/common/clkdev.c
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/changebit.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/clearbit.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/div64.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/findbit.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/memzero.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/setbit.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/testclearbit.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/testchangebit.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/testsetbit.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/io-readsb.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/io-readsl.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/io-readsw-armv4.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/io-writesb.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/io-writesl.S
SRC_S_$(TARGET_DDE) += $(ARCH_DIR)/lib/io-writesw-armv4.S
#SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/kernel/semaphore.c
SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/kernel/traps.c
SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/kernel/io.c
#SRC_C_$(TARGET_DDE) += $(ARCH_DIR)/mm/flush.c
SRC_C_$(TARGET_DDE) += lib/rwsem-spinlock.c
SRC_C_$(TARGET_DDE) += drivers/amba/bus.c
endif

# + contrib stuff / slightly modified stuff
SRC_C_$(TARGET_DDE) += \
                          kernel/exit.c \
                          kernel/kthread.c \
                          kernel/mutex.c \
                          kernel/rcuclassic.c \
                          kernel/notifier.c \
                          kernel/resource.c \
                          kernel/rwsem.c \
                          kernel/sched.c \
                          kernel/semaphore.c \
                          kernel/sys.c \
                          kernel/time.c \
                          kernel/time/timekeeping.c \
                          kernel/timer.c \
                          kernel/wait.c \
                          kernel/workqueue.c \
                          lib/bitmap.c \
                          lib/bitrev.c \
                          lib/crc32.c \
                          lib/ctype.c \
                          lib/cpumask.c \
                          lib/dec_and_lock.c \
                          lib/find_next_bit.c \
                          lib/hexdump.c \
                          lib/idr.c \
                          lib/iomap.c \
                          lib/hweight.c \
                          lib/kasprintf.c \
                          lib/kernel_lock.c \
                          lib/klist.c \
                          lib/kobject.c \
                          lib/kref.c \
                          lib/parser.c \
                          lib/proportions.c \
                          lib/radix-tree.c \
                          lib/rbtree.c \
                          lib/scatterlist.c \
                          lib/sha1.c \
                          lib/string.c \
                          lib/vsprintf.c \
                          mm/dmapool.c \
                          mm/mempool.c \
                          mm/swap.c \
                          mm/util.c \
                          drivers/base/attribute_container.c \
                          drivers/base/bus.c \
                          drivers/base/class.c \
                          drivers/base/core.c \
                          drivers/base/cpu.c \
                          drivers/base/dd.c \
                          drivers/base/devres.c \
                          drivers/base/driver.c \
                          drivers/base/init.c \
                          drivers/base/map.c \
                          drivers/base/platform.c \
                          drivers/base/sys.c

##################################################################
# Sources for libdde_linux_net.a                                 #
##################################################################
SRC_C_$(TARGET_DDE_NET) += \
                              arch/l4/net.c \
                              drivers/net/mii.c \
                              net/core/dev.c \
                              net/core/dev_mcast.c \
                              net/core/ethtool.c \
                              net/core/link_watch.c \
                              net/core/neighbour.c \
                              net/core/netevent.c \
                              net/core/net-sysfs.c \
                              net/core/net_namespace.c \
                              net/core/rtnetlink.c \
                              net/core/skbuff.c \
                              net/core/skb_dma_map.c \
                              net/core/utils.c \
                              net/ethernet/eth.c \
                              net/sched/sch_generic.c

##################################################################
# Sources for libdde_linux_sound.a                               #
##################################################################
SRC_C_$(TARGET_DDE_SOUND) += \
                                sound/sound_core.c \
                                arch/l4/sound.c

##################################################################
# Sources for libdde_linux_block.a                               #
##################################################################
SRC_C_$(TARGET_DDE_BLOCK) += \
                                arch/l4/inodes.c      \
                                block/blk-barrier.c   \
                                block/blk-core.c      \
                                block/blk-exec.c      \
                                block/blk-ioc.c       \
                                block/blk-map.c              \
                                block/blk-merge.c      \
                                block/blk-settings.c      \
                                block/blk-softirq.c \
                                block/blk-sysfs.c      \
                                block/blk-tag.c      \
                                block/blk-timeout.c      \
                                block/cmd-filter.c    \
                                block/elevator.c      \
                                block/genhd.c         \
                                block/noop-iosched.c  \
                                block/ioctl.c         \
                                block/scsi_ioctl.c    \
                                lib/hexdump.c          \
                                lib/bcd.c             \
                                mm/backing-dev.c      \
                                mm/bounce.c           \
                                mm/page-writeback.c   \
                                mm/filemap.c              \
                                fs/bio.c              \
                                fs/block_dev.c        \
                                fs/buffer.c           \
                                fs/filesystems.c      \
                                fs/inode.c               \
                                fs/partitions/check.c

#XXX lib/vsprintf.c is already declared in TARGET_DDE. Don't understand why
# should it be declared in TARGET_DDE_BLOCK

ifeq ($(ARCH), x86)
SRC_C_$(TARGET_DDE_BLOCK) += \
                                fs/partitions/msdos.c
endif

##################################################################
# Sources for libdde_linux_char.a                                #
##################################################################
SRC_C_$(TARGET_DDE_CHAR) += \
                                arch/l4/inodes.c      \
                                fs/char_dev.c

##################################################################
# Sources for libdde_linux_pci.a                                 #
##################################################################
SRC_C_$(TARGET_DDE_PCI) +=  \
                          arch/l4/pci.c \
                          drivers/pci/access.c \
                          drivers/pci/bus.c \
                          drivers/pci/hotplug-pci.c \
                          drivers/pci/pci.c \
                          drivers/pci/pci-driver.c \
                          drivers/pci/probe.c \
                          drivers/pci/search.c \
                          drivers/pci/setup-bus.c \
                          drivers/pci/setup-res.c


##################################################################
# Sources for libdde_linux_fs.a                                  #
##################################################################
SRC_C_$(TARGET_DDE_FS) += \
                                fs/attr.c \
                                fs/bad_inode.c \
                                fs/dcache.c \
                                fs/libfs.c \
                                fs/mpage.c \
                                fs/namei.c \
                                fs/namespace.c \
                                fs/open.c \
                                fs/read_write.c \
                                fs/super.c \
                                fs/splice.c \
                                fs/stat.c

#                                fs/fs-writeback.c
#                                fs/direct-io.c

# # Warning! Following files are also included in _DDE_BLOCK
# SRC_C_$(TARGET_DDE_FS) += \
#                                 fs/block_dev.c

##################################################################
# DDE specific build rules                                       #
##################################################################

all::

lib/crc32.o : crc32table.h
lib/crc32.o : PRIVATE_INCDIR += .
kernel/time.o : timeconst.h
kernel/time.o : PRIVATE_INCDIR += .

timeconst.h : $(SRC_DIR)/kernel/timeconst.pl
	@$(GEN_MESSAGE)
	$(VERBOSE)$< 250 >$@

crc32table.h : gen_crc32table
	@$(GEN_MESSAGE)
	$(VERBOSE)./$< >$@

gen_crc32table : lib/gen_crc32table.c
	@$(GEN_MESSAGE)
	$(VERBOSE)$(HOST_CC) -O2 -o $@ $<

include $(PKGDIR)/linux26/Makeconf

include $(L4DIR)/mk/lib.mk

CFLAGS := $(filter-out -std=gnu99,$(CFLAGS))
