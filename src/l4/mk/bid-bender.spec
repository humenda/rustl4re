incdir      = %:set-var(incdir      %(l4obj)/include/contrib)
pc_file_dir = %:set-var(pc_file_dir %(l4obj)/pc)

# options that take an extra argument
link_arg_opts =
  %:arg-option(L m z o h e -entry fini init -defsym)
  %:arg-option(b -format A -architecture y -trace-symbol MF)
  %:arg-option(-hash-style)
  %:arg-option(T Tbss Tdata Ttext Ttext-segment Trodata-segment Tldata-segment)
  %:arg-option(dT)

# options that are part of the output file list %o
link_output_args =
  %:output-option(l* -whole-archive -no-whole-archive
                  -start-group -end-group)


l4libdir =
l4libdir_x = %:set-var(l4libdir
              %(l4system:%(l4api:%(l4obj)/lib/%(l4system)/%(l4api)))
              %(l4system:%(l4obj)/lib/%(l4system)) %(l4obj)/lib)

# compile a list of dirs from -L options
libdir    = %:set-var(libdir %{L*:%*} %(l4libdir))

# search for libgcc files in the L4-libs and then in the GCC dirs
libgcc    = %:search(libgcc.a %(libdir) %(gcclibdir))
libgcc_eh = %:search(libgcc_eh.a %(libdir) %(gcclibdir))
libgcc_s  = %:search(libgcc.so %(libdir) %(gcclibdir))

# get dependency file name from -MF or from -o options
deps_file = %:strip(%{MF*:%*;:%{o*:.%*.d;:.pcs-deps}})

# generate dependency files for used spec/pc files
generate_deps =
  # main dependency
  %:echo-file(>%(deps_file) %{o*:%*}: %:all-specs())
  # empty deps for all spec/pc files for graceful removal
  %:foreach(%%:echo-file(>>%(deps_file) %%*:) %:all-specs())

# check whether the linker variable is set
check_linker = %(linker:;:%:error(linker variable not defined))


######### ld compatibility (pass through) mode for linking ##################
# options to pass to the linker (binutils ld)
link_pass_opts = %:set-var(link_pass_opts
  %{M} %{-print-map} %{-trace-symbol*} %{y} %{-verbose}
  %{-cref} %{-trace} %{r} %{O*}
  %{m} %{-error-*} %{-warn-*&-no-warn-*}
  %{-sort-*} %{-unique*}
  %{-define-common&-no-define-common} %{B*}
  %{-check-*&-no-check-*}
  %{-no-undefined} %{rpath*} %{-verbose*}
  %{-discard-*}
  %{x} %{X} %{S} %{s} %{t} %{z} %{Z} %{n} %{N} %{init*} %{fini*}
  %{soname*} %{h} %{E} %{-export-dynamic&-no-export-dynamic}
  %{e} %{-entry*} %{-defsym*} %{b} %{-format*} %{A} %{-architecture*}
  %{-gc-sections} %{gc-sections} %{-hash-style*}
  # we always set -nostlib below so drop it but use it to avoid an error
  %{nostdlib:} %{no-pie:} %{pie})

# linker arguments
link_args =
  %(link_arg_opts)%(link_output_args)
  %:read-pc-file(%(pc_file_dir) %{PC*:%*})
  %{nocrt|r:;:%:read-pc-file(%(pc_file_dir) ldscripts)}
  %{o} -nostdlib %{static:-static;:--eh-frame-hdr} %{shared}
  %(link_pass_opts) %:foreach(%%{: -L%%*} %(l4libdir)) %{T*&L*}
  %{!r:%{!dT:-dT %:search(main_%{static:stat;shared:rel;:dyn}.ld %(libdir))}}
  %{r|shared|static|-dynamic-linker*:;:--dynamic-linker=%(Link_DynLinker)
    %(Link_DynLinker:;:
      %:error(Link_DynLinker not specified, cannot link with shared libs.))}
  %{-dynamic-linker*}
  %(Link_Start) %o %{OBJ*:%*} %(Libs)
  %{static:--start-group} %(Link_Libs) %{!shared:%(libgcc);:%(libgcc_s)}
  %(libgcc_eh) %{static:--end-group} %(Link_End)
  %{EL&EB}
  %{MD:%(generate_deps)} %:error-unused-options()

# executed when called as 'ld-l4' (l4 linker)
ld = %(check_linker) %:exec(%(linker) %(link_args))


######### gcc command line compatibility mode for linker ###################
# maps GCC command line options directly to gnu-ld options
# specify command line compatible to linking with GCC
gcc_arg_opts =
  %:arg-option(aux-info param x idirafter include imacro iprefix
               iwithprefix iwithprefixbefore isystem imultilib
               isysroot Xpreprocessor Xassembler T
               Xlinker u z G o U D I MF)

link_output_args_gcc = %:output-option(l*)

# pass all -Wl, and -Xlinker flags as output to the linker, preserving the order
# with all -l and non-option args
link_pass_opts_gcc   = %:set-var(link_pass_opts_gcc %:(%{Wl,*&Xlinker*:%w%*}))

link_args_gcc =
  %(gcc_arg_opts)%(link_output_args_gcc)
  %{pie:}%{no-pie:}%{nostdlib:}%{static:}%{shared:}%{nostdinc:}
  %{std*:} %{m*:}
  %:read-pc-file(%(pc_file_dir) %{PC*:%*})
  %{r}
  %{r|nocrt|nostartfiles|nostdlib:;:%:read-pc-file(%(pc_file_dir) ldscripts)}
  %{o} -nostdlib %{static:-static;:--eh-frame-hdr} %{shared}
  %(link_pass_opts_gcc) %{W*:} %{f*:} %{u*} %{O*} %{g*} %{T*&L*}
  %{!r:%{!dT:-dT %:search(main_%{static:stat;shared:rel;:dyn}.ld %(libdir))}}
  %{r|shared|static|-dynamic-linker*:;:--dynamic-linker=%(Link_DynLinker)
    %(Link_DynLinker:;:
      %:error(Link_DynLinker not specified, cannot link with shared libs.))}
  %{r|nostartfiles|nostdlib:;:%(Link_Start)} %o %(Libs)
  %{r|nodefaultlibs|nostdlib:;:%{static:--start-group} %(Link_Libs)
  %{!r:%{!shared:%(libgcc);:%(libgcc_s)} %(libgcc_eh) %{static:--end-group}}}
  %{r|nostartfiles|nostdlib:;:%(Link_End)}
  %{EL&EB}
  %{MD:%(generate_deps)} %:error-unused-options()

# executed when called with '-t ld' (l4 linker)
gcc-ld = %(check_linker) %:exec(%(linker) %(link_args_gcc))


################## GCC pass through for linking host / l4linux mode ###########
# implementation for modes 'host' and 'l4linux' that use GCC/G++ as linker
# (we use gcc as linker in that case)
link_host_mode_args =
  %(gcc_arg_opts)
  %:read-pc-file(%(pc_file_dir) %{PC*:%*})
  %{o} %{z} %{pie&no-pie} %{v} %{g*} %{-coverage} %{undef}
  %{static} %o
  %{I*&D*&U*} %{L*&l*&Wl,*&Xlinker*} %<Wl,*
  %{!static:-Wl,-Bstatic} -Wl,--start-group %(Libs) %(Link_Libs) -Wl,--end-group
  %{!static:-Wl,-Bdynamic}
  %{EL&EB} %{m*} %{W*} %{f*}
  %{MD:%(generate_deps)} %:error-unused-options()

# executed when called with '-t host-ld', host linker.
host-ld =  %(check_linker) %:exec(%(linker) %(link_host_mode_args))

