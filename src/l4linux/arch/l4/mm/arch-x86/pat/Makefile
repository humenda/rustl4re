# SPDX-License-Identifier: GPL-2.0

obj-y				:= set_memory.o memtype.o

obj-$(CONFIG_X86_PAT)		+= memtype_interval.o

include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../../../x86/mm/pat,   \
       set_memory memtype memtype_interval)
