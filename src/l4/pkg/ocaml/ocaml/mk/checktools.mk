
CMD_CAMLOPT := $(shell sh -c "command -v $(CAMLOPT)")
ifeq ($(CMD_CAMLOPT),)
TARGET :=
text := $(shell echo "\033[32mHost tool '$(CAMLOPT)' missing, skipping build.\033[0m")
$(info $(text))
else
  CMD_CAMLOPT := $(shell sh -c "command -v ocamlrun")
  ifneq ($(shell file $(CMD_CAMLOPT) | grep x86-64),)
    TARGET :=
    text := $(shell echo "\033[32mHost tool '$(CAMLOPT)' is 64bit, cannot build 32bit targets, skipping build.\033[0m")
    $(info $(text))
  endif
endif

ifeq ($(shell sh -c "command -v $(CAMLC)"),)
TARGET :=
text := $(shell echo "\033[32mHost tool '$(CAMLC)' missing, skipping build.\033[0m")
$(info $(text))
endif
