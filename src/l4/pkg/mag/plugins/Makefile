PKGDIR  ?= ..
L4DIR   ?= $(PKGDIR)/../..
TARGET   = src client_fb input_libinput input_event background mag_client \
           session_manager

include $(L4DIR)/mk/subdir.mk

$(filter-out src,$(TARGET)): src
