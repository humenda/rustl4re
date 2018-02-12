#
# Makefile for l4lxlib
#

lib-y := task.o thread.o memory.o misc.o

lib-$(CONFIG_L4_IRQ_MULTI)  += irq_multi.o
lib-$(CONFIG_L4_IRQ_SINGLE) += irq.o
lib-$(CONFIG_L4_VCPU)       += irq_vcpu.o
