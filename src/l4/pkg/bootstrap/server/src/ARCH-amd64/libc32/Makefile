PKGDIR   ?= ../../../..
L4DIR    ?= $(PKGDIR)/../..

TARGET    = libc32.a

SYSTEMS   = amd64


SRC_C	  = __assert_fail.c __lltostr.c __ltostr.c __v_printf.c \
	    isspace.c memcpy.c memmove.c memset.c printf.c puts.c \
	    strtol.c strtoul.c vprintf.c

include $(L4DIR)/mk/lib.mk

CFLAGS	 := $(filter-out $(CCXX_FLAGS) $(CARCHFLAGS), $(CFLAGS)) -mno-sse
CPPFLAGS := -nostdinc -I$(SRC_DIR)/include -m32
OPTS     := -Os
