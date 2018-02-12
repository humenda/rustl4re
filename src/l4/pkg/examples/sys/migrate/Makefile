PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET                       = ex_thread_migrate ex_thread_migrate_irq
SRC_CC_ex_thread_migrate     = thread_migrate.cc
SRC_CC_ex_thread_migrate_irq = thread_migrate_irq.cc
REQUIRES_LIBS                = libpthread

include $(L4DIR)/mk/prog.mk
