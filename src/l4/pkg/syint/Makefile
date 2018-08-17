PKGDIR	= .
L4DIR	?= $(PKGDIR)/../..

TARGET		= libsyint-cc/ src

include $(L4DIR)/mk/subdir.mk

test: # this does not compile our tests before
	@python3 qemu_master.py
