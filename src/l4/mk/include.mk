# -*- Makefile -*-
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Template for include directories
#
# supported targets:
#
#   all				- the default, link the includes into the
#				  local include dir
#   install			- install the includes into the global
#				  include dir
#   config			- do nothing, may be overwritten


INSTALLDIR_INC		?= $(DROPS_STDDIR)/include
INSTALLDIR_INC_LOCAL	?= $(OBJ_BASE)/include
INSTALLDIR		= $(INSTALLDIR_INC)
INSTALLDIR_LOCAL	= $(INSTALLDIR_INC_LOCAL)

ifeq ($(origin TARGET),undefined)
# use POSIX -print here
TARGET_CMD		= (cd $(INCSRC_DIR) \
                            && find . -name '*.[ih]' -print \
                            $(if $(EXTRA_TARGET),&& echo $(EXTRA_TARGET)))
else
TARGET_CMD		= echo $(TARGET)
endif
ifeq ($(CONTRIB_HEADERS),)
INSTALL_INC_PREFIX	?= l4/$(PKGNAME)
else
INSTALL_INC_PREFIX	?= contrib/$(PKGNAME)
endif
INCSRC_DIR		?= $(SRC_DIR)

include $(L4DIR)/mk/Makeconf
$(GENERAL_D_LOC): $(L4DIR)/mk/include.mk
-include $(DEPSVAR)

do_link = if (readlink($$dst) ne $$src) {                                     \
            if ($$notify eq 1) {                                              \
              $$notify=0; $(if $(VERBOSE),print "  ... Updating symlinks\n";,)\
            }                                                                 \
            system("ln","-sf$(if $(VERBOSE),,v)",$$src,$$dst) && exit 1;      \
          }
do_inst = system("install","-$(if $(VERBOSE),,v)m","644",$$src,$$dst) && exit 1;
installscript = perl -e '                                                     \
  chomp($$srcdir="$(INCSRC_DIR)");                                            \
  $$notify=1;                                                                 \
  while(<>) {                                                                 \
    foreach (split) {                                                         \
      s|^\./||; $$src=$$_;                                                    \
      if(s|^ARCH-([^/]*)/L4API-([^/]*)/([^ ]*)$$|\1/\2/$(INSTALL_INC_PREFIX)/\3| ||\
	 s|^ARCH-([^/]*)/([^ ]*)$$|\1/$(INSTALL_INC_PREFIX)/\2| ||            \
	 s|^L4API-([^/]*)/([^ ]*)$$|\1/$(INSTALL_INC_PREFIX)/\2| ||           \
	 s|^(/.*/)?(\S*)$$|$(INSTALL_INC_PREFIX)/\2|) {                       \
	    $$src="$$srcdir/$$src" if $$src !~ /^\//;                         \
	    $$dstdir=$$dst="$(if $(1),$(INSTALLDIR_LOCAL),$(INSTALLDIR))/$$_";\
	    $$dstdir=~s|/[^/]*$$||;                                           \
	    -d $$dstdir || system("install","-$(if $(VERBOSE),,v)d",$$dstdir) && exit 1;        \
	    $(if $(1),$(do_link),$(do_inst))                                  \
	  }                                                                   \
    }                                                                         \
  }'

PC_FILENAMES ?= $(PC_FILENAME)
PC_FILES     := $(foreach pcfile,$(PC_FILENAMES),$(OBJ_BASE)/pc/$(pcfile).pc)

$(OBJ_BASE)/pc/%.pc: $(GENERAL_D_LOC)
	$(VERBOSE)$(call generate_pcfile,$*,$@,$(PKGNAME),,$(call get_cont,REQUIRES_LIBS,$*))

headers::

all:: headers $(PC_FILES)
	@$(TARGET_CMD) | $(call installscript,1)

install::
	@$(INSTALL_LINK_MESSAGE)
	@$(TARGET_CMD) | $(call installscript);

cleanall::
	$(VERBOSE)$(RM) .general.d

help::
	@echo "  all            - install files to $(INSTALLDIR_LOCAL)"
	@echo "  install        - install files to $(INSTALLDIR)"
	@echo "  scrub          - delete backup and temporary files"
	@echo "  clean          - same as scrub"
	@echo "  cleanall       - same as scrub"
	@echo "  help           - this help"

scrub clean cleanall::
	$(VERBOSE)$(SCRUB)
