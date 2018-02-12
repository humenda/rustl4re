PKGDIR ?= ../../
L4DIR  ?= $(PKGDIR)/../..

TARGET = libpython.a
PC_FILENAME = libpython
REQUIRES_LIBS := readline
WARNINGS       = $(WARNINGS_MINIMAL) -Wno-unused-but-set-variable \
                 -Wno-unused-function

PRIVATE_INCDIR = $(PKGDIR_ABS)/include $(PKGDIR_ABS)/include/ARCH-$(ARCH)

##########################################################################
# Parser
POBJS = Parser/acceler.c \
		Parser/grammar1.c \
		Parser/listnode.c \
		Parser/node.c \
		Parser/parser.c \
		Parser/parsetok.c \
		Parser/bitset.c \
		Parser/metagrammar.c \
		Parser/firstsets.c \
		Parser/grammar.c \
		Parser/pgen.c

PARSER_OBJS = $(POBJS) Parser/myreadline.c Parser/tokenizer.c

##########################################################################
# Objects
UNICODE_OBJS=   Objects/unicodeobject.c Objects/unicodectype.c


OBJECT_OBJS=	\
		Objects/abstract.c \
		Objects/boolobject.c \
		Objects/bufferobject.c \
		Objects/bytes_methods.c \
		Objects/bytearrayobject.c \
		Objects/cellobject.c \
		Objects/classobject.c \
		Objects/cobject.c \
		Objects/codeobject.c \
		Objects/complexobject.c \
		Objects/descrobject.c \
		Objects/enumobject.c \
		Objects/exceptions.c \
		Objects/genobject.c \
		Objects/fileobject.c \
		Objects/floatobject.c \
		Objects/frameobject.c \
		Objects/funcobject.c \
		Objects/intobject.c \
		Objects/iterobject.c \
		Objects/listobject.c \
		Objects/longobject.c \
		Objects/dictobject.c \
		Objects/methodobject.c \
		Objects/moduleobject.c \
		Objects/object.c \
		Objects/obmalloc.c \
		Objects/rangeobject.c \
		Objects/setobject.c \
		Objects/sliceobject.c \
		Objects/stringobject.c \
		Objects/structseq.c \
		Objects/tupleobject.c \
		Objects/typeobject.c \
		Objects/weakrefobject.c \
		$(UNICODE_OBJS)

##########################################################################
# Python

THREADOBJ = Python/thread.c
DYNLOADFILE=dynload_shlib.c

PYTHON_OBJS =	\
		Python/_warnings.c \
		Python/Python-ast.c \
		Python/asdl.c \
		Python/ast.c \
		Python/bltinmodule.c \
		Python/ceval.c \
		Python/compile.c \
		Python/codecs.c \
		Python/errors.c \
		Python/frozen.c \
		Python/frozenmain.c \
		Python/future.c \
		Python/getargs.c \
		Python/getcompiler.c \
		Python/getcopyright.c \
		Python/getmtime.c \
		Python/getplatform.c \
		Python/getversion.c \
		Python/graminit.c \
		Python/import.c \
		Python/importdl.c \
		Python/marshal.c \
		Python/modsupport.c \
		Python/mystrtoul.c \
		Python/mysnprintf.c \
		Python/peephole.c \
		Python/pyarena.c \
		Python/pyfpe.c \
		Python/pymath.c \
		Python/pystate.c \
		Python/pythonrun.c \
		Python/structmember.c \
		Python/symtable.c \
		Python/sysmodule.c \
		Python/traceback.c \
		Python/getopt.c \
		Python/pystrcmp.c \
		Python/pystrtod.c \
		Python/formatter_unicode.c \
		Python/formatter_string.c \
		$(DYNLOADFILE) \
		$(THREADOBJ)

##########################################################################
# Modules
MODULE_OBJS=	\
		config.c \
		getpath.c \
		Modules/main.c \
		Modules/gcmodule.c

MODOBJS = Modules/threadmodule.c \
		  Modules/signalmodule.c \
		  Modules/posixmodule.c \
		  Modules/errnomodule.c \
		  Modules/pwdmodule.c \
		  Modules/_sre.c \
		  Modules/_codecsmodule.c \
		  Modules/zipimport.c \
		  Modules/symtablemodule.c \
		  Modules/xxsubtype.c
EXTRA_MODULES = \
		  Modules/readline.c \
          Modules/timemodule.c

SRC_C = Modules/getbuildinfo.c
SRC_C += $(PARSER_OBJS)
SRC_C += $(OBJECT_OBJS)
SRC_C += $(PYTHON_OBJS)
SRC_C += $(MODULE_OBJS)
SRC_C += $(MODOBJS)
SRC_C += $(EXTRA_MODULES)
SRC_C += dummies.c

vpath %.c $(PKGDIR)/contrib

PRIVATE_INCDIR += $(PKGDIR)/contrib/Include $(PKGDIR)/contrib/Python

include $(L4DIR)/mk/lib.mk
