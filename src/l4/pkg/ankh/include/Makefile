PKGDIR	?= ..
L4DIR	?= $(PKGDIR)/../..

# All haeder files found in this directory tree will be automatically
# installed in a way that they can be included with
# #include <l4/pkgname/yourfile.h> later.
# No need to list them in this Makefile.

EXTRA_TARGET += lock \
				protocol \
				session  \
				shm

include $(L4DIR)/mk/include.mk
