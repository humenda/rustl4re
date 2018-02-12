PKGDIR	= .
L4DIR	?= $(PKGDIR)/../..

TARGET = include lib interpreter

include $(L4DIR)/mk/subdir.mk

interpreter: lib

clean_paranoid: 
		$(RM) -rf $(OBJ_DIR)
