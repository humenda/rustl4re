PKGDIR   ?= ../..
L4DIR    ?= $(PKGDIR)/../..


USE_ASYNC_FE := n
TARGET       := cons
SRC_CC       := controller.cc mux_impl.cc main.cc client.cc vcon_client.cc \
                vcon_fe_base.cc vcon_fe.cc registry.cc

SRC_CC-$(USE_ASYNC_FE)  += async_vcon_fe.cc
DEFINES-$(USE_ASYNC_FE) := -DUSE_ASYNC_FE

REQUIRES_LIBS                 = libstdc++ cxx_libc_io cxx_io
REQUIRES_LIBS-$(USE_ASYNC_FE) = libpthread

SRC_CC        += $(SRC_CC-y)
REQUIRES_LIBS += $(REQUIRES_LIBS-y)
DEFINES       += $(DEFINES-y)

include $(L4DIR)/mk/prog.mk
