PKGDIR ?= ..
L4DIR ?= $(PKGDIR)/../..

INSTALL_TARGET = memcheck-x86-l4re none-x86-l4re

include $(L4DIR)/mk/Makeconf

# path- and version-specific variables
VALGRIND_VERSION = 3.6.0-svn
VALGRIND_SRC = $(shell find $(PKGDIR)/src/ -name valgrind-$(VALGRIND_VERSION))
VALGRIND_BUILD = $(OBJ_DIR)/OBJ-$(SYSTEM)/vg

# basic BID settings
REQUIRES_LIBS    = l4re_c-util l4util

# variables used for passing to Valgrind's build system
# gcc need a "," after defsym and a "=" after dynamic-linker
L4_LDFLAGS_WITH_LIBC=`echo "$(LDFLAGS)" | \
            sed 's/'--dynamic-linker\ '/'-Wl,--dynamic-linker\='/g' | \
            sed 's/'--defsym\ '/'-Wl,--defsym\,'/g' | \
            sed 's/'--start-group\ '/'-Wl,--start-group\ '/g' | \
            sed 's/'--end-group\ '/'-Wl,--end-group\ '/g' | \
            sed 's$$'-T$(LDS_stat_bin)'$$''$$g'`
L4_CFLAGS  = $(CFLAGS)

L4_CRT0 = $(CRTI_stat_bin) $(CRTBEGIN_stat_bin)

SYSTEMS = x86-l4f

include $(L4DIR)/mk/prog.mk

ifneq ($(SYSTEM), )
ifeq ($(BUILD_ARCH), x86)

TMP_LDFLAGS = $(addprefix -L, $(L4LIBDIR))
TMP_LDFLAGS += -Wl,--start-group
TMP_LDFLAGS += -Wl,--defsym,__L4_KIP_ADDR__=$(L4_KIP_ADDR) \
               -Wl,--defsym,__L4_STACK_ADDR__=$(L4_STACK_ADDR)
TMP_LDFLAGS += $(REQUIRES_LIBS_LIST)
TMP_LDFLAGS += -Wl,--end-group
TMP_LDFLAGS += -Wl,--warn-common -gc-sections
L4_LDFLAGS = $(TMP_LDFLAGS)

$(INSTALL_TARGET): valgrind_build
	$(VERBOSE)$(LN) -sf vg/none/none-x86-l4re
	$(VERBOSE)$(LN) -sf vg/memcheck/memcheck-x86-l4re

valgrind_build: check_rediff valgrind_conf
	@echo -e "\033[35;1m___...---=== COMPILING ===---...___\033[0m"
	L4_CFLAGS="$(L4_CFLAGS)" \
	L4_CPPFLAGS="$(CPPFLAGS)" \
	L4_CXXFLAGS="$(CXXFLAGS)" \
	L4_CRT0="$(L4_CRT0)" \
	L4_CRT1="$(CRT1_stat_bin)" \
	L4_CRTN="$(CRTN)" \
	L4_CFLAGS="$(L4_CFLAGS)" \
	L4_CPPFLAGS="$(CPPFLAGS)" \
	L4_LDFLAGS="$(L4_LDFLAGS)" \
	L4_LDFLAGS_WITH_LIBC="$(L4_LDFLAGS_WITH_LIBC)" \
	L4_OBJ_BASE="$(OBJ_BASE)" \
	L4_LDS_stat_bin="$(LDS_stat_bin)" \
	VG_LDFLAGS="$(VG_LDFLAGS)" \
	LD="$(LINK_PROGRAM)" \
	AR="$(AR)" \
		make -C $(VALGRIND_BUILD)

valgrind_conf:
	if [ ! -e $(VALGRIND_BUILD)/config.ready ]; then \
		echo -e "\033[35;1m___...---=== CONFIG ===---...___\033[0m" && \
		echo OBJ: $(VALGRIND_BUILD) && \
		mkdir -p $(VALGRIND_BUILD) && \
		cd $(VALGRIND_BUILD) &&  \
		AR="$(AR)" CC="$(CC)" CXX="$(CXX)" STRIP="$(STRIP)" RANLIB="$(RANLIB)"\
		$(VALGRIND_SRC)/configure \
						  --enable-maintainer-mode \
						  --host=i386-l4re \
						  --build=i386-linux && \
		touch $(VALGRIND_BUILD)/config.ready; \
	fi

# this rule checks whether the modified file copies from other l4 packages
# are still up-to-date
check_rediff:
	@shacmd=shasum; command -v $$shacmd 2>&1 /dev/null || shacmd=sha1sum; \
	cd $(PKGDIR)/.. && $$shacmd -c $(SRC_DIR)/ext-checksums; \
	if [ $$? != 0 ]; then \
	  echo "Current checksums:"; \
	  while read c f; do \
	    $$shacmd $$f; \
	  done < $(SRC_DIR)/ext-checksums; \
	  exit 1; \
	fi


.PHONY = valgrind_conf valgrind_build

endif # ARCH_x86
endif # SYSTEM != ""

clean cleanall::
	@echo -e "\033[35;1m___...---=== CLEAN ===---...___\033[0m"
	$(RM) -r $(OBJ_DIR)/*

