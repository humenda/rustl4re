# vim:se ts=4:
PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libhistory.a libreadline.a

CONTRIB_INCDIR = readline

PRIVATE_INCDIR = $(PKGDIR)/lib/readline 			\
				 $(PKGDIR)/include 					\
				 $(PKGDIR)/lib/contrib/libreadline \
			 	 # this - is kind of crazy: expands to -I- which
			  	 # means not to search for include files within
				 # the file`s directory even if the file made some
				 # #include "header.h"

WARNINGS	= -Wno-missing-prototypes

DEFINES		= -DHAVE_CONFIG_H -D__L4

#DEBUG		= 1

vpath % 	$(PKGDIR)/lib/readline \
			$(PKGDIR)/lib/contrib/libreadline

HISTFILES	= history.c histfile.c histexpand.c histsearch.c xmalloc.c \
			  shell.c

SRC_C_libhistory.a		= $(HISTFILES)
SRC_C_libreadline.a		=	readline.c 	funmap.c 	keymaps.c \
							vi_mode.c 	parens.c	bind.c\
							isearch.c	display.c 	emacs_keymap.c\
							vi_keymap.c util.c 		kill.c\
							undo.c		macro.c		input.c\
							callback.c	nls.c		savestring.c\
							tilde.c 	complete.c $(HISTFILES)\
							rltty.c 	terminal.c  signals.c \
							search.c	replacements.c

include $(L4DIR)/mk/lib.mk
