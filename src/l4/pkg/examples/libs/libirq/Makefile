PKGDIR ?=	../..
L4DIR ?=	$(PKGDIR)/../..

SRC_C_ex_libirq_async = async_isr.c
SRC_C_ex_libirq_loop  = loop.c
TARGET                = ex_libirq_async ex_libirq_loop
DEPENDS_PKGS          = libirq
REQUIRES_LIBS         = libirq libio

include $(L4DIR)/mk/prog.mk
