include arch/arm/vfp/Makefile

# we copied the .c and .S files so that they take the modified vfpinstr.h
# file from this directory, for no other reason
# make sure the copy is pristine
always-y += __check_vfp

$(obj)/__check_vfp: $(wildcard $(srctree)/arch/*/vfp/*.[cS]) FORCE
	$(Q)for f in $(srctree)/arch/l4/vfp/*.[cS]; do \
	  if ! cmp $(srctree)/arch/arm/vfp/$${f##*/} $$f; then \
	    diff -u $(srctree)/arch/arm/vfp/$${f##*/} $$f; \
	    echo ; \
	    echo "File $$f changed. Fix it."; \
	    exit 1; \
	  fi; \
	done
