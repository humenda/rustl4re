PKGDIR 	?= ..
L4DIR	?= $(PKGDIR)/../..

TARGET = contrib include lib examples

include $(L4DIR)/mk/subdir.mk

include: contrib
lib:      contrib include
examples: lib

.PHONY: lib examples contrib include
