PKGDIR          ?= ..
L4DIR           ?= $(PKGDIR)/../..

TARGET           = dash
REQUIRES_LIBS    = libc_be_sig libc_support_misc libc_be_file_stdin

PRIVATE_INCDIR   = $(PKGDIR)/contrib/src $(SRC_DIR) $(L4DIR)
DEFINES          = -DBSD=1 -DSHELL -DHAVE_CONFIG_H
CFLAGS           = -include $(SRC_DIR)/config.h
WARNINGS         = $(WARNINGS_MINIMAL) -Wno-unused-but-set-variable

vpath %.c   $(PKGDIR)/contrib/src

SRC_C            = alias.c arith_yacc.c arith_yylex.c cd.c error.c eval.c \
                   exec.c expand.c histedit.c input.c jobs.c mail.c \
		   main.c memalloc.c miscbltin.c mystring.c options.c \
		   parser.c redir.c show.c trap.c output.c bltin/printf.c \
		   system.c bltin/test.c bltin/times.c var.c syntax.c \
		   builtins.c nodes.c signames.c init.c

include $(L4DIR)/mk/prog.mk
