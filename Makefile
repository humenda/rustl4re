all: 
	@if [ -d obj ]; then                                           \
	  $(MAKE) build_all;                                           \
	else                                                           \
	  echo "Call 'make setup' once for initial setup." ;           \
	  exit 1;                                                      \
	fi

clean:
	@$(RM) -r obj

setup:
	@if [ -d obj ]; then                                                            \
	  echo "Snapshot has already been setup. Proceed with 'make setup' or 'make clean'.";    \
	else                                                                            \
	  export PATH=$$(pwd)/bin:$$PATH;                                               \
	  chmod +x ./bin/setup.d/*;                                                     \
	  for binary in bin/setup.d/*; do                                               \
	    ./$$binary config || exit 1;                                                \
	  done;                                                                         \
	  for binary in bin/setup.d/*; do                                               \
	    ./$$binary setup || exit 1;                                                 \
	  done;                                                                         \
	  echo ====================================================================;    \
	  echo ;                                                                        \
	  echo Your L4Re tree is set up now. Type 'make' to build the tree. This;       \
	  echo will take some time \(depending on the speed of your host system, of;    \
	  echo course\).;                                                               \
	  echo ;                                                                        \
	  echo Boot-images for ARM targets will be placed into obj/l4/arm-*/images.;    \
	  echo Boot-images for MIPS targets will be placed into obj/l4/mips32/images.;    \
	  echo Check obj/l4/.../conf/Makeconf.boot for path configuration during image builds.; \
	  echo ;                                                                        \
	fi

build_all: build_fiasco build_l4re build_l4linux build_images

#.NOTPARALLEL: build_fiasco build_l4re build_l4linux build_images build_all

build_fiasco: $(addsuffix /fiasco,$(wildcard obj/fiasco/*))
build_l4re: $(addsuffix /l4defs.mk.inc,$(wildcard obj/l4/*))
build_l4linux: $(addsuffix /vmlinux,$(wildcard obj/l4linux/*))

$(addsuffix /fiasco,$(wildcard obj/fiasco/*)):
	$(MAKE) -C $(@D)

$(addsuffix /l4defs.mk.inc,$(wildcard obj/l4/*)):
	$(MAKE) -C $(@D)

obj/l4linux/amd64/vmlinux: obj/l4/amd64/l4defs.mk.inc
	$(MAKE) -C src/l4linux O=$(abspath $(@D)) x86_64-mp_vPCI_defconfig
	src/l4linux/scripts/config --file $(@D)/.config \
	                           --set-str L4_OBJ_TREE $(abspath obj/l4/amd64)
# temporary quick fix
	src/l4linux/scripts/config --file $(@D)/.config -d IA32_EMULATION -d X86_X32 -e PCI -e L4_VPCI
	$(MAKE) -C $(@D) olddefconfig
	$(MAKE) -C $(@D)

obj/l4linux/ux/vmlinux: obj/l4/x86/l4defs.mk.inc
	$(MAKE) -C src/l4linux O=$(abspath $(@D)) x86_32-ux_defconfig
	src/l4linux/scripts/config --file $(@D)/.config \
	                           --set-str L4_OBJ_TREE $(abspath obj/l4/x86)
	$(MAKE) -C $(@D)

obj/l4linux/arm-mp/vmlinux: obj/l4/arm-v7/l4defs.mk.inc
	+. obj/.config && $(MAKE) -C src/l4linux L4ARCH=arm \
	                          CROSS_COMPILE=$$CROSS_COMPILE_ARM \
	                          O=$(abspath $(@D)) arm-mp_defconfig
	src/l4linux/scripts/config --file $(@D)/.config \
	                           --set-str L4_OBJ_TREE $(abspath obj/l4/arm-v7)
	+. obj/.config && $(MAKE) -C $(@D) CROSS_COMPILE=$$CROSS_COMPILE_ARM

obj/l4linux/arm-up/vmlinux: obj/l4/arm-v7/l4defs.mk.inc
	+. obj/.config && $(MAKE) -C src/l4linux L4ARCH=arm \
	                          CROSS_COMPILE=$$CROSS_COMPILE_ARM \
	                          O=$(abspath $(@D)) arm_defconfig
	src/l4linux/scripts/config --file $(@D)/.config \
	                           --set-str L4_OBJ_TREE $(abspath obj/l4/arm-v7)
	+. obj/.config && $(MAKE) -C $(@D) CROSS_COMPILE=$$CROSS_COMPILE_ARM

obj/l4linux/arm64/vmlinux: obj/l4/arm64/l4defs.mk.inc
	+. obj/.config && $(MAKE) -C $(@D) CROSS_COMPILE=$$CROSS_COMPILE_ARM64

build_images: build_l4linux build_l4re build_fiasco
	@echo "=============== Building Images ==============================="
	export PATH=$$(pwd)/bin:$$PATH;                                        \
	[ -e obj/.config ] && . obj/.config;                                   \
	for d in obj/l4/*; do                                                  \
	  if [ -d "$$d" -a -e $$d/.imagebuilds ]; then                         \
	    cat $$d/.imagebuilds | while read args; do                         \
	      $(MAKE) -C $$d uimage $$args;                                    \
	    done;                                                              \
	  fi;                                                                  \
	done	
	@echo "=============== Build done ===================================="

gen_prebuilt: copy_prebuilt pre-built-images/l4image

copy_prebuilt2:
	@echo "Creating pre-built image directory"
	@cd obj/l4;                                          \
	for arch in *; do                                    \
	  for i in $$arch/images/*; do \
	    if [ "$$arch" = "amd64" -o "$$arch" = "x86" ]; then \
	      mkdir -p ../../pre-built-images/$$arch; \
	      if [ -d $$i ]; then \
	        for f in $$i/*.elf; do \
	          cp $$f ../../pre-built-images/$$arch; \
	        done; \
	      fi; \
	    else \
	      if [ -d $$i ]; then \
		pt=$${i#$$arch/images/}; \
		mkdir -p ../../pre-built-images/$$arch/$$pt; \
		for f in $$i/*.elf $$i/*.uimage; do \
		  cp $$f ../../pre-built-images/$$arch/$$pt; \
		done; \
	      fi; \
	    fi; \
	  done; \
	done

copy_prebuilt:
	@echo "Creating pre-built image directory"
	@cd obj/l4;                                          \
	for arch in *; do                                    \
	  mkdir -p ../../pre-built-images/$$arch;            \
	  for i in $$arch/images/*.elf                       \
	           $$arch/images/*.uimage; do                \
	    [ -e $$i ] || continue;                          \
	    if [ $$i != $$arch/images/bootstrap.elf -a       \
	         $$i != $$arch/images/bootstrap.uimage -a    \
	         $$i != amd64/images/bootstrap32.elf ]; then \
	      cp $$i ../../pre-built-images/$$arch;          \
	    fi;                                              \
	  done;                                              \
	done

pre-built-images/l4image:
	@echo Creating $@
	@src/l4/tool/bin/l4image --create-l4image-binary $@

help:
	@echo "Targets:"
	@echo "  all"
	@echo "  setup"
	@echo "  gen_prebuilt"

.PHONY: setup all build_all clean help \
        build_images build_fiasco build_l4re build_l4linux
