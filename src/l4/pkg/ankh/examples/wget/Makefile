PKGDIR          ?= ../..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = wget_clnt
SYSTEMS          = x86-l4f arm-l4f
REQUIRES_LIBS    = liblwip_netif_ankh libc_be_fs_noop libc_support_misc libc_be_sig

PRIVATE_INCDIR  += $(SRC_DIR)/contrib/src
CFLAGS          += -DHAVE_CONFIG_H -Wno-sign-compare -Wno-unused

vpath %.c $(SRC_DIR)/contrib/src

SRC_C = \
		connect.c \
		convert.c \
		cookies.c \
		ftp.c \
		ftp-basic.c \
		ftp-ls.c \
		hash.c \
		html-parse.c \
		html-url.c \
		host.c \
		http.c \
		init.c \
		l4re_helpers.c \
		log.c \
		main.c \
		netrc.c \
		progress.c \
		ptimer.c \
		recur.c \
		res.c \
		retr.c \
		safe-ctype.c \
		spider.c \
		url.c \
		utils.c \
		version.c \
		xmalloc.c

include $(L4DIR)/mk/prog.mk
