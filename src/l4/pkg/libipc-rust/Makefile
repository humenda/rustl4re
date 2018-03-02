PKGDIR ?= .
L4DIR  ?= $(PKGDIR)/../..

TARGET  = libipc.rlib
SRC_C   = lib.rs

#pre-rust-library-compilation:
#	@echo set environment variable from package to OUTPUT_DIR and save result to bindings.rs
#	@echo $(PKGDIR_OBJ)
#	bindgen -o $(PKGDIR_OBJ)/bindings.rs *.h -- -DSYSTEM_amd64_gen_l4f -DARCH_amd64 -DCPUTYPE_gen -DL4API_l4f -D_GNU_SOURCE nclud -I/home/streicher/uni/af/Armageddon/obj/l4/amd64/include/amd64/l4f -I/home/streicher/uni/af/Armageddon/obj/l4/amd64/include/amd64 -I/home/streicher/uni/af/Armageddon/obj/l4/amd64/include -I/home/streicher/uni/af/Armageddon/obj/l4/amd64/include/uclibc 
#	touch $@
include $(L4DIR)/mk/lib.mk

