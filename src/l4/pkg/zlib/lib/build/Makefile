PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libl4z.a libl4z.so
LIBS            = $(GCCLIB)
WARNINGS        = $(WARNINGS_MEDIUM) \
                  $(call checkcc,-Wno-implicit-function-declaration)
SYSTEMS	        = $(SYSTEMS_ABI) $(SYSTEMS_PLAIN)
CONTRIB_INCDIR	= zlib
PC_FILENAME     = $(if $(filter %-l4f,$(SYSTEM)),zlib)

SRC_C		= adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c \
                  gzread.c gzwrite.c infback.c inffast.c inflate.c inftrees.c \
		  trees.c uncompr.c zutil.c
DEBUG 		= 0

vpath %.c $(SRC_DIR)/../contrib

include $(L4DIR)/mk/lib.mk
