MODE		?= shared

REQUIRES_LIBS	+= scout

SRC_DATA += $(SRC_BIN)
SRC_CC		= doc.cc
vpath %.txt $(SRC_DIR)

include $(L4DIR)/mk/prog.mk

$(GENERAL_D_LOC): $(SCOUTDIR)/mk/scout.mk

doc.cc: $(SRC_TXT)
	@$(GEN_MESSAGE)
	$(VERBOSE)$(GOSH) --style $(SCOUTDIR)/mk/scout.gosh $< >$@ || rm $@

