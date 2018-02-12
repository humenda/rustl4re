PKGDIR ?= ../..
L4DIR  ?= $(PKGDIR)/../../..

TARGET   := libio-vbus.a libio-vbus.so

SRC_CC   := vbus.cc vbus_pci.cc vbus_gpio.cc vbus_pm.cc
#vbus_i2c.cc vbus_mcspi.cc
CXXFLAGS += -DL4_NO_RTTI -fno-rtti -fno-exceptions
PC_FILENAME := libio-vbus

include $(L4DIR)/mk/lib.mk
