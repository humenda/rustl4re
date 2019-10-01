# Rust On L4Re

This repository contains the L4Re snapshot from July 2018 and is organised
into two branches. The master branch contains all the modifications to L4Re's
build system BID to make Rust work in the first place. Another branch, called `pkg` contains all the applications and
libraries built so far.

Suggestions, criticism and everything else is welcome, contact me on IRC as
Moomoc in the Mozilla, Freenode or OFTC network.

L4Re Setup
----------

This is better explained in [RL4Re's README](EADME.l4re), but in short you need
(GNU) make, gcc, libgcc, bison and flex.

This L4Re snapshot needs to reside in a path without any spaces, thanks to Makes
inability to treat those appropriately. Enter the L4Re source tree root and type
`make ssetup` to choose the build target. Then type make, followed by half a
dozen tea breaks.

Now you need to configure the object directory for your build. Open a new file
called `obj/l4/amd64/conf/Makeconf.local` and specify the object directory; substitute
amd64 through the chosen architecture. Assuming you are
on `x86_64` (AKA amd64) and the source tree is below `/home/user/l4re`, the
content might look like this:

    OBJ_BASE=/home/USER/rustl4re/obj/l4/amd64

Rust Compilation
----------------

Rust doesn't ship binary versions of the cross-compiled `std` crate. There is
also a minor deficiency in passing linker arguments containing spaces, so you
need a slightly patched Rust compiler yourself. Grab the source from
<https://github.com/humenda/rust> and follow these steps:

-   Create a file `config.toml` in the repository root with this content:

    ````
    [llvm]
    optimize = true
    [build]
    build = "x86_64-unknown-linux-gnu"    # defaults to your host platform
    target = ["x86_64-unknown-l4re-uclibc"] # defaults to just the build triple
    docs = false
    [install]
    prefix = '/your/preferred/path'
    [rust]
    optimize = true
    use-jemalloc = false
    backtrace = false

    You should adjust both prefix and potentially the `build =`-line, if you
    don't use Linux on a `x86_64` machine.
-   For building Rust with std:

    $ python3 x.py --host=x86_64-unknown-linux-gnu --target=x86_64-unknown-l4re-uclibc


Then you can proceed to building the L4Re source tree, see the linked
instructions above.
