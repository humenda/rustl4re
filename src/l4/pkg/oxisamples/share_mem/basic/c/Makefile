PKGDIR  ?= ..
L4DIR   ?= $(PKGDIR)/../../../..


TARGET		= shm_c-client shm_c-server
SRC_CC_shm_c-server		= server.cc
SRC_CC_shm_c-client		= client.cc

REQUIRES_LIBS    = libpthread libc_be_sig l4re_c-util libc_support_misc l4re_c

include $(L4DIR)/mk/prog.mk
