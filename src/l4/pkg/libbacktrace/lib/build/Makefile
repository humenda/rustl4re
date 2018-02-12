PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET          = libbacktrace.a libbacktrace.so
CONTRIB_INCDIR  = libbacktrace
PRIVATE_INCDIR  = $(SRC_DIR) $(SRC_DIR)/../contrib/include
SRC_C           = atomic.c backtrace.c dwarf.c elf.c fileline.c mmap.c \
                  mmapio.c posix.c print.c simple.c sort.c state.c
SRC_C          += helper.c
NOT_HAVE_SYNC_FUNCTIONS-sparc = y
DEFINES         = $(if $(NOT_HAVE_SYNC_FUNCTIONS-$(ARCH)),,-DHAVE_SYNC_FUNCTIONS=1)

vpath %.c $(SRC_DIR)/../contrib/libbacktrace

include $(L4DIR)/mk/lib.mk
