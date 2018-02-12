PKGDIR  ?= ../..
L4DIR   ?= $(PKGDIR)/../..

TARGET        = libc_be_file_stdin.a libc_be_file_stdin.so
LINK_INCR     = libc_be_file_stdin.a
PC_FILENAME   = libc_be_file_stdin
REQUIRES_LIBS = libevent l4re-util
SRC_CC        = stdin.cc

include $(L4DIR)/mk/lib.mk
