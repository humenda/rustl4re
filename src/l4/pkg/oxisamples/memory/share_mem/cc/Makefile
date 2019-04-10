PKGDIR	= .
L4DIR	?= $(PKGDIR)/../../../../..

TARGET		= shm-client shm-server
SRC_CC_shm-client		= client.cc
SRC_CC_shm-server		= server.cc

REQUIRES_LIBS    = libdl libpthread libc_be_sig l4re_c-util libc_support_misc l4re_c

include $(L4DIR)/mk/prog.mk
