# -*- Makefile -*-
# vim:set ft=make:
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-include to extract variables from Makefile-compatible files
#
# We use this file to browse doxygen files in the doc.mk rules.

BID_print:
	@echo $($(VAR))
BID_printm:
	@$(foreach v, $(VARS), echo '$(v)=$($(v))';)

.PHONY: BID_print BID_printm
