PKGDIR         ?= ../..
L4DIR          ?= $(PKGDIR)/../..

TARGET          = l4vio_net_p2p
REQUIRES_LIBS   = libstdc++ l4virtio
PRIVATE_INCDIR += $(PKGDIR)/server/include
SRC_CC          = net.cc

include $(L4DIR)/mk/prog.mk
