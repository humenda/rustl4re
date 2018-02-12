

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

build_all:
	@echo "=============== Building all Fiasco configurations ============"
	@export PATH=$$(pwd)/bin:$$PATH;                                       \
	for f in obj/fiasco/*; do                                              \
	  if [ -d "$$f" ]; then                                                \
	    echo " ============ Building in $$f ========= ";                   \
	    if ! $(MAKE) -C $$f V=0; then                                      \
	      echo "Error building the Fiasco '$$f' variant.";                 \
	      echo "Press RETURN to continue with other variants.";            \
	      read ;                                                           \
	    fi                                                                 \
	  fi                                                                   \
	done
	@echo "=============== Building all L4Re   configurations ============"
	@export PATH=$$(pwd)/bin:$$PATH;                                       \
	for f in obj/l4/*; do                                                  \
	  if [ -d "$$f" ]; then                                                \
	    echo " ============ Building in $$f ========= ";                   \
	    if ! $(MAKE) -C $$f V=0; then                                      \
	      echo "Error building the L4Re '$$f' variant.";                   \
	      echo "Press RETURN to continue with other variants.";            \
	      read ;                                                           \
	    fi                                                                 \
	  fi                                                                   \
	done
	@echo "=============== Building all L4Linux configurations ==========="
	@export PATH=$$(pwd)/bin:$$PATH;                                       \
	[ -e obj/.config ] && . obj/.config;                                   \
	for f in obj/l4linux/*; do                                             \
	  if [ -d "$$f" ]; then                                                \
	    echo " ============ Building in $$f ========= ";                   \
	    tmp=$${f##*/};                                                     \
	    build_for_arch=x86;                                                \
	    [ "$${tmp#arm}" != "$$tmp" ] && build_for_arch=arm;                \
	    [ "$$build_for_arch" = "arm" -a -n "$$SKIP_L4LINUX_ARM_BUILD" ]    \
	       && continue;                                                    \
	    if ! $(MAKE) L4ARCH=$$build_for_arch -C $$f; then                  \
	      echo "Error building the L4Linux '$$f' variant.";                \
	      echo "Press RETURN to continue with other variants.";            \
	      read ;                                                           \
	    fi                                                                 \
	  fi                                                                   \
	done
	@echo "=============== Building Image ================================"
	@export PATH=$$(pwd)/bin:$$PATH;                                       \
	[ -e obj/.config ] && . obj/.config;                                   \
	for d in obj/l4/*; do                                                  \
	  if [ -d "$$d" -a $${d#obj/l4/arm-} != "$$d" ]; then                  \
	    $(MAKE) -C $$d elfimage E=hello ;                                  \
	    $(MAKE) -C $$d elfimage E=hello-shared ;                           \
	    [ -n "$$SKIP_L4LINUX_ARM_BUILD" -o ! -d obj/l4linux/arm-mp ]       \
	      || $(MAKE) -C $$d elfimage E="L4Linux-basic" ;                   \
	  fi;                                                                  \
	  if [ -d "$$d" -a "$${d##obj/l4/mips}" != "$$d" ]; then               \
	    $(MAKE) -C $$d elfimage E=hello ;                                  \
	    $(MAKE) -C $$d elfimage E=hello-shared ;                           \
	  fi                                                                   \
	done	
	@echo "=============== Build done ===================================="

.PHONY: setup all build_all clean
