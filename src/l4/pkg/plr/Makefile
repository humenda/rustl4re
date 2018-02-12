PKGDIR	?= .
L4DIR	?= $(PKGDIR)/../..

# the default is to build the listed directories, provided that they
# contain a Makefile. If you need to change this, uncomment the following
# line and adapt it.
# TARGET = include src lib server doc

SYSTEMS = x86-l4f

TARGET = include server lib ex

ex : lib server

include $(L4DIR)/mk/subdir.mk
