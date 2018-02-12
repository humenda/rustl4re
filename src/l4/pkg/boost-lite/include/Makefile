PKGDIR         ?= ..
L4DIR          ?= $(PKGDIR)/../..

PKGNAME         = boost-1_39-lite
CONTRIB_HEADERS = y
TARGET          = $(shell find . -name '*.hpp' -print)
#$(info TARGET=$(TARGET))

include $(L4DIR)/mk/include.mk


.PHONY: copy_files
copy_files:
	./gen_include_list file.list > include-list
	if [ -z "$$COPY_FROM_OBJDIR" ]; then             \
	  echo "!!!!! COPY_FROM_OBJDIR not set !!!!!";   \
	  exit 1;                                        \
	fi
	rsync -avzPHS --include-from=include-list \
	  $$COPY_FROM_OBJDIR/include/contrib/boost-1_39/ .
	$(RM) include-list
