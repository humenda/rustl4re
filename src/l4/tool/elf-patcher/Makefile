L4DIR ?= ../..
include $(L4DIR)/mk/Makeconf

CFLAGS	= -O3 $(HOST_CPPFLAGS)
TARGET  = elf-patcher

CXX	= $(HOST_CXX)

all:: $(OBJ_DIR)/$(TARGET)

$(OBJ_DIR)/%: $(SRC_DIR)/%.cc $(SRC_DIR)/Makefile $(SRC_DIR)/%.h
	@$(COMP_MESSAGE)
	$(VERBOSE)$(CXX) $(CFLAGS) -Wall -W -Wextra $< -o $@

clean cleanall::
	@$(CLEAN_MESSAGE)
	$(VERBOSE)rm -f $(OBJ_DIR)/.*.d $(OBJ_DIR)/*.o
	$(VERBOSE)rm -f $(OBJ_DIR)/$(TARGET)

cleanall::
	@$(CLEANALL_MESSAGE)
	$(VERBOSE)rm -f $(TARGET)

