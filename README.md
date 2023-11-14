# Rust On L4Re

This repository contains a patched version of the L4Re microkernel operating
system that includes support for Rust client and server applications. At its
heart, the [L4Rust libraries](src/l4/pkg/l4rust) enable a Rust developer to
efficiently develop Rust clients and servers running on top of a highly
configurable, secure and performant OS. Rust services and clients can
communicate without adaptation with C++ server and clients from L4Re.

L4Re Setup
----------

This is better explained in [RL4Re's README](EADME.l4re), but in short you need
(GNU) make, gcc, libgcc, bison and flex. Execute `make setup` and `make -j8`
afterwards. If compilation aborts with "failed to build l4linux", you
successfully build everything except l4linux which isn't required.

This L4Re snapshot needs to reside in a path without any spaces, thanks to Makes
inability to treat those appropriately. Enter the L4Re source tree root and type
`make setup` to choose the build target. Then type make, followed by half a
dozen tea breaks.

Rust Compilation
----------------

Rust doesn't ship binary versions of the cross-compiled `std` crate. There is
also a minor deficiency in passing linker arguments containing spaces, so you
need a slightly patched Rust compiler yourself. Grab the source from
<https://github.com/hargonix/rust/tree/l4re_fix> (this is in the process of getting upstreamed)
and follow these steps:

-   Create a file `config.toml` in the repository root with this content:
```
changelog-seen = 2
[build]
build = "x86_64-unknown-linux-gnu"
# We need both linux and l4re because we have build.rs scripts in our libraries
target = ["x86_64-unknown-l4re-uclibc", "x86_64-unknown-linux-gnu"]
docs = false
extended = true
[install]
prefix = '/your/prefix/path'
[rust]
optimize = true
jemalloc = false
backtrace = false
```
- `$ python3 x.py build && python3 x.py install`
- `$ rustup toolchain link l4re /your/prefix/path`
- `$ rustup default l4re`

Then you can proceed to building the L4Re source tree, see the linked
instructions above or with hacking, see below.
