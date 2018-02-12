PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= libl4con-idl.a
MODE		= l4env
SYSTEMS		= x86-l4f arm-l4f amd64-l4f
CLIENTIDL	= l4con.idl stream.idl
SERVERIDL	= l4con.idl stream.idl
CPPFLAGS	+= -DL4_MAX_RPC_STRING_DOPES=3

include $(L4DIR)/mk/lib.mk
