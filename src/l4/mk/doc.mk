# -*- Makefile -*-
#
# DROPS (Dresden Realtime OPerating System) Component
#
# Makefile-Template for doc directories
#
# install.inc is used, see there for further documentation

ifeq ($(origin _L4DIR_MK_DOC_MK),undefined)
_L4DIR_MK_DOC_MK=y

ROLE = doc.mk

include $(L4DIR)/mk/Makeconf
$(GENERAL_D_LOC): $(L4DIR)/mk/doc.mk

ifeq ($(IN_OBJ_DIR),)
##################################################################
#
# Empty IN_OBJ_DIR means we are in the source directory and have 
# to first generate a Makefile in the build-dir.
#
##################################################################

all install clean cleanall help:: $(OBJ_DIR)/Makefile.build
	$(VERBOSE)PWD=$(OBJ_DIR) $(MAKE) -C $(OBJ_DIR) O=$(OBJ_BASE) -f Makefile.build $@


$(OBJ_DIR)/Makefile.build: $(SRC_DIR)/Makefile
	$(VERBOSE)install -d $(dir $@)
	$(VERBOSE)echo 'IN_OBJ_DIR=1'                 > $@
	$(VERBOSE)echo 'L4DIR=$(L4DIR_ABS)'          >> $@
	$(VERBOSE)echo 'SRC_DIR=$(SRC_DIR)'          >> $@
	$(VERBOSE)echo 'OBJ_BASE=$(OBJ_BASE)'        >> $@
	$(VERBOSE)echo 'PKGDIR_ABS=$(PKGDIR_ABS)'    >> $@
	$(VERBOSE)echo 'vpath %.fig $(SRC_DIR)'      >> $@
	$(VERBOSE)echo 'vpath %.tex $(SRC_DIR)'      >> $@
	$(VERBOSE)echo 'include $(SRC_DIR)/Makefile' >> $@
									
else
###################################################################
#
# We are in the build directory and can process the documentation
# 
###################################################################

# default is to install all targets
INSTALL_TARGET_MASK ?= %

ifneq ($(TARGET),)
# if no SRC_DOX is given, but TARGET, extract it from TARGET
ifeq ($(origin SRC_DOX),undefined)
SRC_DOX		   := $(filter $(addsuffix .cfg, $(TARGET)),$(wildcard *.cfg))
ifneq ($(SRC_DOX),)
$(error SRC_DOX is undefined, but TARGET is defined. This is invalid since 04/23/2003)
endif
endif
# the same for SRC_TEX
ifeq ($(origin SRC_TEX),undefined)
SRC_TEX		   := $(filter $(TARGET:.ps=.tex),$(wildcard *.tex)) \
		      $(filter $(TARGET:.pdf=.tex),$(wildcard *.tex))
		      $(filter $(TARGET:.dvi=.tex),$(wildcard *.tex))
ifneq ($(SRC_TEX),)
$(error SRC_TEX is undefined, but TARGET is defined. This is invalid since 04/23/2003)
endif
endif
endif

TARGET_DOX 	= $(SRC_DOX:.cfg=) $(SRC_DOX_REF:.cfg=) \
		  $(SRC_DOX_GUIDE:.cfg=) $(SRC_DOX_INT:.cfg=)
INSTALL_TARGET_DOX ?= $(filter $(INSTALL_TARGET_MASK), $(TARGET_DOX))
TARGET_TEX 	?= $(SRC_TEX:.tex=.ps) $(SRC_TEX:.tex=.pdf)
DEPS		+= $(foreach x,$(SRC_TEX:.tex=.dvi),$(dir $x).$(notdir $x).d)

# if no TARGET is given, generate it from all types of targets
TARGET	   	?= $(TARGET_DOX) $(TARGET_TEX)
DEPS		+= $(foreach file,$(TARGET),$(dir $(file)).$(notdir $(file)).d)

all:: $(TARGET)
$(TARGET): $(OBJ_DIR)/.general.d

####################################################################
#
# Doxygen specific
#
####################################################################
DOXY_FLAGS += $(DOXY_FLAGS_$@)

OUTPUTDIR = $(shell perl -n -e '/^\s*OUTPUT_DIRECTORY\s*=\s*(\S+)/ && print "$$1\n"' $(1))

# we refer to %/html sometimes. However, make fails on a rule of the form
# "% %/html:%.cfg", thus the workaround (others than static-pattern-rules
# won't work)
$(addprefix $(OBJ_DIR)/,$(addsuffix /html,$(TARGET_DOX))):$(OBJ_DIR)/%/html:$(TARGET_DOX)

# all our packages
ifeq ($(ALL_SUBDIRS),)
ALL_SUBDIRS := $(shell find -L $(L4DIR)/pkg -maxdepth 4 -type d ! -name .svn -exec test -f {}/Control \; -printf %P' ' -prune)
endif

# We can give an internal rule for doxygen, as the directory specified
# in the config-file should be the name of the config file with the
# .cfg removed.
# Use make DOXY_FAST=y to just build the HTML without graphics
# Use make DOXY_FULL=y to build HTMl with graphics and the PDF
# $(VERBOSE)$(ECHO) ENABLED_SECTIONS=WORKING_SUBPAGES >> $@.flags
FORCE: ;
$(OBJ_DIR)/% $(OBJ_DIR)/%/html:$(SRC_DIR)/%.cfg FORCE
        #generate the flags-file
	$(VERBOSE)$(MKDIR) $@
	$(VERBOSE)$(ECHO) '@INCLUDE_PATH=/'                                 > $@.flags
	$(VERBOSE)$(ECHO) '@INCLUDE=$(SRC_DIR)/$(notdir $<)'               >> $@.flags
	$(VERBOSE)$(ECHO) 'INCLUDE_PATH += $(OBJ_BASE)/include'            >> $@.flags
	$(VERBOSE)$(ECHO) $(DOXY_FLAGS)                                    >> $@.flags
	$(VERBOSE)$(ECHO) OUTPUT_DIRECTORY=$(OBJ_DIR)/$(call OUTPUTDIR,$<) >> $@.flags
	$(VERBOSE)if [ -n "$(DOXY_FAST)" ]; then $(ECHO) HAVE_DOT=NO;  $(ECHO) GENERATE_LATEX=NO;  fi   >> $@.flags
	$(VERBOSE)if [ -n "$(DOXY_FULL)" ]; then $(ECHO) HAVE_DOT=YES; $(ECHO) GENERATE_LATEX=YES; fi   >> $@.flags
	$(VERBOSE)if [ -n "$(DOXY_RELEASE)" ]; then \
	            $(ECHO) HAVE_DOT=YES; \
	            $(ECHO) GENERATE_LATEX=YES; \
		    $(ECHO) SHOW_FILES=YES; \
	            $(ECHO) INTERNAL_DOCS=NO; \
	            $(ECHO) GENERATE_TODOLIST=NO; \
	            $(ECHO) GENERATE_TESTLIST=NO; \
	            $(ECHO) GENERATE_BUGLIST=NO; \
	            $(ECHO) HIDE_UNDOC_CLASSES=YES; \
	            $(ECHO) HIDE_UNDOC_MEMBERS=YES; \
	          fi   >> $@.flags
	$(VERBOSE)cd $(L4DIR)/pkg && \
	  for f in $(ALL_SUBDIRS); \
	    do [ ! -e $(L4DIR)/pkg/$$f/doc/files.cfg ] || sed -e "s,%PKGDIR%,$(L4DIR)/pkg/$$f,g" $(L4DIR)/pkg/$$f/doc/files.cfg || true; done >> $@.flags
	$(VERBOSE)cd $(OBJ_BASE)/include && L4DIR=$(L4DIR) $(DOXYGEN) $@.flags
	$(VERBOSE)for file in $(ADD_FILES_TO_HTML); do cp $$file $@/html; done
	$(VERBOSE)( [ -r $@/latex/Makefile ] && \
	   echo | PWD=$@/latex $(MAKE) -C $@/latex ) || true
	$(VERBOSE)if [ -d $@ ] ; then touch $@ ; fi

# Installation rules follow
#
# define LOCAL_INSTALLDIR prior to including install.inc, where the install-
# rules are defined. Same for INSTALLDIR.
INSTALLDIR_HTML		?= $(DROPS_STDDIR)/doc/html
INSTALLFILE_HTML	?= $(CP) -pR $(1) $(2)
INSTALLDIR_HTML_LOCAL	?= $(OBJ_BASE)/doc/html
INSTALLFILE_HTML_LOCAL	?= $(if $(call is_dir,$(2)), \
                              find '$(dir $(1))' -name '$(notdir $(1))' | xargs $(LN) -t $(2) -sf, \
                              find '$(dir $(1))' -name '$(notdir $(1))' | xargs -I '{}' $(LN) -sf '{}' $(2))

INSTALLDIR		= $(INSTALLDIR_HTML)
INSTALLFILE		= $(INSTALLFILE_HTML)
INSTALLDIR_LOCAL	= $(INSTALLDIR_HTML_LOCAL)
INSTALLFILE_LOCAL	= $(INSTALLFILE_HTML_LOCAL)

all::	$(TARGET) \
        $(addprefix $(INSTALLDIR_LOCAL)/, $(addsuffix .title, $(INSTALL_TARGET_DOX)))

$(OBJ_DIR)/$(SRC_DOX_REF:.cfg=.title): BID_DOC_DOXTYPE=ref
$(OBJ_DIR)/$(SRC_DOX_GUIDE:.cfg=.title): BID_DOC_DOXTYPE=guide
$(OBJ_DIR)/$(SRC_DOX_INT:.cfg=.title): BID_DOC_DOXTYPE=int

# first line: type
# second line: title that will appear at the generated index page
$(OBJ_DIR)/%.title:$(SRC_DIR)/%.cfg $(OBJ_DIR)/.general.d
	$(VERBOSE)$(ECHO) $(BID_DOC_DOXTYPE)>$@
	$(VERBOSE)MAKEFLAGS= $(MAKE) -s -f $(L4DIR)/mk/makehelpers.inc -f $< \
	   BID_print VAR=PROJECT_NAME >>$@

# Install the title file locally
# The installed title file depends on the installed doku for message reasons
$(foreach f,$(INSTALL_TARGET_DOX),$(INSTALLDIR_LOCAL)/$(f).title):$(INSTALLDIR_LOCAL)/%.title:$(OBJ_DIR)/%.title $(INSTALLDIR_LOCAL)/%
	$(VERBOSE)$(call INSTALLFILE_LOCAL,$<,$@)

# Install the docu locally, the title file will depend on
$(foreach f,$(INSTALL_TARGET_DOX),$(INSTALLDIR_LOCAL)/$(f)):$(INSTALLDIR_LOCAL)/%:$(OBJ_DIR)/% $(OBJ_DIR)/%/html
	@$(INSTALL_DOC_LOCAL_MESSAGE)
	$(VERBOSE)$(INSTALL) -d $@
	$(VERBOSE)$(call INSTALLFILE_LOCAL,$</html/*,$@) 

# Install the title file globally
# The installed title file depends on the installed doku for message reasons
$(foreach f,$(INSTALL_TARGET_DOX),$(INSTALLDIR)/$(f).title):$(INSTALLDIR)/%.title:$(OBJ_DIR)/%.title $(INSTALLDIR)/%
	$(VERBOSE)$(call INSTALLFILE,$<,$@)

# Install the docu globally, the title file will depend on
$(foreach f,$(INSTALL_TARGET_DOX),$(INSTALLDIR)/$(f)):$(INSTALLDIR)/%:$(OBJ_DIR)/% $(OBJ_DIR)/%/html
	@$(INSTALL_DOC_MESSAGE)
	$(if $(INSTALLFILE),$(VERBOSE)$(INSTALL) -d $@)
	$(VERBOSE)$(call INSTALLFILE,$</html/*,$@)

install:: $(addprefix $(INSTALLDIR)/,$(addsuffix .title,$(INSTALL_TARGET_DOX)))
.PHONY: $(addprefix $(INSTALLDIR)/,$(INSTALL_TARGET_DOX) \
				   $(addsuffix .title,$(INSTALL_TARGET_DOX)))


#################################################################
#
# Latex specific
#
#################################################################

FIG2EPS_PROG ?= fig2dev -L eps
FIG2PDF_PROG ?= fig2dev -L pdf
FIG2PNG_PROG ?= fig2dev -L png

$(SRC_TEX:.tex=.dvi) $(TARGET): $(SRC_FIG:.fig=.pdf) $(SRC_FIG:.fig=.png) $(SRC_FIG:.fig=.eps)

%.eps: %.fig $(OBJ_DIR)/.general.d
	@$(GEN_MESSAGE)
	$(VERBOSE)$(FIG2EPS_PROG) $< $@

%.pdf: %.fig $(OBJ_DIR)/.general.d
	@$(GEN_MESSAGE)
	$(VERBOSE)$(FIG2PDF_PROG) $< $@

%.png: %.fig $(OBJ_DIR)/.general.d
	@$(GEN_MESSAGE)
	$(VERBOSE)$(FIG2PNG_PROG) $< $@

%.ps: %.dvi $(OBJ_DIR)/.general.d
	@$(GEN_MESSAGE)
	$(VERBOSE)$(call MAKEDEP,dvips) dvips -o $@ $<
	$(VERBOSE)$(VIEWERREFRESH_PS)

%.pdf: %.tex $(OBJ_DIR)/.general.d
	@$(GEN_MESSAGE)
	$(VERBOSE)$(PDFLATEX) $< || \
		(($(GREP) 'TeX capacity exceeded' $*.log && \
		echo -e "\n\033[31mIncrease pool_size to 200000 in" \
		"/etc/texmf/texmf.cnf!\033[m\n" && false) || false)
	$(VERBOSE)$(GREP) '\citation' $*.aux && \
		bibtex $* || true
	$(VERBOSE)(export size=1; touch $@; \
		until [ $$size -eq `ls -o $@ | awk '{print $$4}'` ]; do\
		  export size=`ls -o $@ | awk '{print $$4}'` ;\
		  $(PDFLATEX) $< ;\
		done)
# one more time, just to be sure ...
	$(VERBOSE)$(PDFLATEX) $<

%.dvi: %.tex $(OBJ_DIR)/.general.d
	@$(GEN_MESSAGE)
	$(VERBOSE)$(call MAKEDEP,$(LATEX)) $(LATEX) $<
	$(VERBOSE)if grep -q '\indexentry' $*.idx; then makeindex $*; fi
	$(VERBOSE)if grep -q '\citation' $*.aux; then bibtex $*; fi
        # Do we really need to call latex unconditionally again? Isn't it
        # sufficient to check the logfile for the "rerun" text?
	$(VERBOSE)$(LATEX) $<
	$(VERBOSE)latex_count=5 ; \
	while egrep -s 'Rerun (LaTeX|to get cross-references right)' $*.log &&\
		[ $$latex_count -gt 0 ] ; do \
		$(LATEX) $< \
		let latex_count=$$latex_count-1 ;\
	done
	$(VERBOSE)$(VIEWERREFRESH_DVI)

SHOWTEX ?= $(firstword $(SRC_TEX))
SHOWDVI ?= $(SHOWTEX:.tex=.dvi)
SHOWPS  ?= $(SHOWTEX:.tex=.ps)
SHOWPDF ?= $(SHOWTEX:.tex=.pdf)

VIEWER_DVI	  ?= xdvi
VIEWER_PS         ?= gv
VIEWER_PDF	  ?= xpdf
VIEWERREFRESH_DVI ?= killall -USR1 xdvi xdvi.bin xdvi.real || true
VIEWERREFRESH_PS  ?= killall -HUP $(VIEWER_PS) || true

dvi:	$(SHOWDVI)
show showdvi: dvi
	$(VERBOSE)$(VIEWER_DVI) $(SHOWDVI) &

ps:	$(SHOWPS)
showps: ps
	$(VERBOSE)$(VIEWER_PS) $(SHOWPS) &

pdf:	$(SHOWPDF)
showpdf: pdf
	$(VERBOSE)$(VIEWER_PDF) $(SHOWPDF) &

clean::
	$(VERBOSE)$(RM) $(addprefix $(OBJ_DIR)/, \
		$(addsuffix .title,$(TARGET_DOX)))
	$(VERBOSE)$(RM) $(addprefix $(OBJ_DIR)/, \
		$(addsuffix .flags,$(TARGET_DOX)))
	$(VERBOSE)$(RM) $(wildcard $(addprefix $(OBJ_DIR)/, $(foreach ext, \
		aux bbl blg dvi idx ilg ind log lod ltf toc out, \
		$(SRC_TEX:.tex=.$(ext))) texput.log ))

cleanall:: clean
	$(VERBOSE)$(RM) -r $(wildcard $(addprefix $(OBJ_DIR)/, \
		$(TARGET)) $(wildcard .*.d))
	$(VERBOSE)$(RM) $(wildcard $(addprefix $(OBJ_DIR)/, \
		$(SRC_TEX:.tex=.ps) $(SRC_TEX:.tex=.pdf)))
	$(VERBOSE)$(RM) $(wildcard $(addprefix $(OBJ_DIR)/, \
		$(SRC_FIG:.fig=.eps) $(SRC_FIG:.fig=.pdf) $(SRC_FIG:.fig=.png)))
	$(VERBOSE)$(RM) $(wildcard $(addprefix $(OBJ_DIR)/, \
		Makefile Makefile.build))

.PHONY: all clean cleanall config help install oldconfig txtconfig
.PHONY: ps pdf dvi showps showpdf showdvi show

help::
	@echo "Specify a target:"
	@echo "all       - generate documentation and install locally"
ifneq (,$(INSTALL_TARGET_DOX))
	@echo "install   - generate documentation and install globally"
endif
	@echo "dvi       - compile the primary TeX file into dvi"
	@echo "showdvi   - invoke the dvi viewer on the primary TeX file"
	@echo "ps        - compile the primary TeX file into ps"
	@echo "showps    - invoke the ps viewer on the primary TeX file"
	@echo "pdf       - compile the primary TeX file into pdf"
	@echo "showpdf   - invoke the pdf viewer on the primary TeX file"
	@echo "clean     - delete generated intermediate files"
	@echo "cleanall  - delete all generated files"
	@echo "help      - this help"
	@echo
ifneq (,$(SRC_TEX))
	@echo "Primary TeX file: $(SHOWTEX)"
	@echo "Other documentation to be built: $(filter-out $(SHOWPDF) $(SHOWPS) $(SHOWDVI),$(TARGET))"
else
ifneq (,$(TARGET_DOX))
	@echo "Primary Doxygen file: $(addsuffix .cfg, $(TARGET_DOX))"
endif
	@echo "Documentation to be built: $(TARGET)"
endif

-include $(DEPSVAR)

endif   # IN_OBJ_DIR

endif	# _L4DIR_MK_DOC_MK undefined
