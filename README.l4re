
This file shall briefly describe the ways how to handle the snapshot.

We would really like that you read all of the README before asking
questions, although this file might be longer than you (and we) want.
Thanks a lot in advance.



Host system requirements
========================

The host system shall be a x86 32bit or 64bit based system with a recent
Linux distribution installed and at least 2GB of free disk space.

All necessary tools required by the build are available from the provided
packages of the Linux distributions, including cross compilers. But
there are also other cross compiler packages available (see below).
You might want to run `make check_build_tools` in the src/l4
directory to verify the common tools are installed.

You are free to use any Linux distribution you like, or even BSDs or any of
its derivatives. But then you should know the game. Especially tool
versions should be recent, as installed on the listed distributions below.

We are confident that the snapshot works on the following distributions:

* Debian 9 or later
* Ubuntu 16.10 or later


Cross Compilers
===============

Cross Compiling for ARM
-----------------------

For compiling software for the ARM targets on an x86 host a cross
compiler is needed.

For ARM:

Two popular prebuilt cross compilers exist. You can also check
http://elinux.org/ARMCompilers for more info:

* Linaro cross-compilers:

     [https://linaro.org/downloads](https://linaro.org/downloads)

  Note that the Linaro cross compilers build for hard floating point, so
  builds with those compilers will only work on appropriate targets.
  This compiler will not work for ARMv5/ARM9 targets.

* Codesourcery / Mentor cross-compilers:
  See [https://elinux.org/ARMCompilers](https://elinux.org/ARMCompilers) to
  get an arm-2014.05 version. Alternatively go via
  [https://sourcery.mentor.com/GNUToolchain/](https://sourcery.mentor.com/GNUToolchain/).

* Distributions:

  Linux distributions, such as Debian, also provide cross compilers.

* Alternatively:

  You have your own gcc cross compiler with at least version 4.7 installed
  that is known to be working.


Cross compiling for MIPS
------------------------

 For compiling software for MIPS on an x86 host a cross compiler is needed.

 Download cross compilers from:

 [https://www.mips.com/develop/tools/codescape-mips-sdk/download-codescape-mips-sdk-essentials/](https://www.mips.com/develop/tools/codescape-mips-sdk/download-codescape-mips-sdk-essentials/)

 Note, when compiling for r2, use the "mti" variant.
 When compiling for r6, use the "img" variant.

 Other cross compiler builds can also work. Any (positive + negative)
 feedback welcome.


Using the Cross Compiler
------------------------

Cross compilers are used via the common `CROSS_COMPILE` variable.
`make setup` also asks for a `CROSS_COMPILE` prefix to be used for
a specific build.



Building
========

In the upper most directory of the archive (the same directory where this
README is located) contains a Makefile. To setup the snapshot issue

    make setup

and to build it issue

    make

in the snapshot directory. Add -j X as appropriate.

Directory layout
================

* `bin/`
   Configuration logics for this snapshot.

* `doc/`

   `source/`
     Contains documentation automatically generated from the documented
     source code. Overview documentation is also included there.
     l4re-doc-full.pdf and l4re-doc-base.pdf: PDF file of the generated code
     html: HTML version of the documentation. Can be viewed in any recent
           web browser.
* `obj/`
   Generated object code will be placed under the obj directory.

   `fiasco/`
     Fiasco build directories.

   `l4/`
     L4Re build drectories.

   `l4linux/`
     L4Linux build directories.

* `src/`
   Contains the source code:
   `kernel/fiasco`: Fiasco source
   `l4`:            L4Re source
   `l4linux`:       L4Linux

* files/
   Miscellaneous files.
    * `ramdisk-x86.rd`:    Ramdisks for (L4)Linux.
    * `ramdisk-arm.rd`:    Ramdisks for (L4)Linux.
    * `ramdisk-amd64.rd`:  Ramdisks for (L4)Linux.

All object directories are built by default.

Serial Console
==============

If you happen to use Windows as your workstation OS to connect to your
target machine we recommend using PuTTY (free, open source tool, ask your
favorite search engine) as a terminal emulator. Hyperterm is not
recommended because it is basically unable to display all the output
properly.

On Linux hosts the situation is much more relaxed, minicom and PuTTY are
known to work, as probably any other solution.

QEMU
====

To run the built system under QEMU, go to an appropriate
`obj/l4`-directory of your choice, such as `obj/l4/x86`, and run:

    make qemu

This will display a dialog menu to let you choose an entry to boot. For
example, choose 'hello' and you should see the system starting and finally
see "Hello World" scroll by periodically.

Configuring yourself
====================

The `make setup` step configures predefined setups for both the
L4Re microkernel (Fiasco) and the L4Re user-level software, and
connects both together so the images for the target system can be
built.

Of course, you can also do this yourself for your specific targets.

Generally, the microkernel is built for a very specific target, i.e. it is
build for a SoC, such as ARM's Zedboard based on the Xilinx Zynq platform,
or the MIPS Baikal-T.

In contrast, L4Re is built for the architecture and possibly
sub-architecture (CPU variant). Again referring to the Zedboard and
Baikal-T, L4Re would be compiled for the ARMv7-A ARM CPU variant and
MIPS32r2 variant respectively.


Configure the L4Re microkernel aka Fiasco 
-----------------------------------------

Within the snapshot layout build directories for Fiasco are created under
`obj/fiasco`. To create a build directory, go to `src/kernel/fiasco` and do:

    $ cd src/kernel/fiasco
    $ make B=../../../obj/fiasco/builddir
    Creating build directory "../../../obj/fiasco/builddir"...
    done.

This will have created a build directory, go there and configure it
according to your requirements:

    $ cd ../../../obj/fiasco/builddir
    $ make config

`make config` will open up a configuration menu. Configure Fiasco as
required. Finally save the configuration and build:

    $ make -j4

When successful, this will create a file `fiasco` in the build directory.


Configure L4Re User-Level Infrastructure
----------------------------------------

Within the snapshot layout build directories for the L4Re user-level
infrastructure are under `obj/l4`. To create a build directory, go to
`src/l4` and do:

    $ cd src/l4
    $ make B=../../obj/l4/builddir

This will have created a build directory, go there and configure
it according to your requirements:

    $ cd ../../obj/l4/builddir
    $ make config

`make config` will open up a configuration menu. Configure as
required. Finally save the configuration build:

    $ make -j4

Building will compile all the components of L4Re, however, it will not build
an image that you can load on the target.


Pulling it together
-------------------

For creating images to load on the target, the image building step
needs to know where all the files can be found to include in the image.
The image contains all the executable program files of the setup to build,
including the Fiasco kernel, but also other files that are necessary
to run the setup, such as configuration files, ramdisks, or data files.

The image building step is integrated in the l4re build system. All
relevant configuration settings for building an image are
taken from `src/l4/conf/Makeconf.boot`. A template is available
as `src/l4/conf/Makeconf.boot.example`, and it is encouraged that you
copy that file to `src/l4/conf/Makeconf.boot`.

The most relevant variable in that file is `MODULE_SEARCH_PATH` which
defines where the image building process shall look for files. This variable
has absolute paths separated with either spaces or colons (':').
For the examples to work, we need to add the path to the Fiasco
build directory as you have chosen in the above building step.
Change the line accordingly.

When done, you can proceed to build an image. Go to the l4 build directory
and create an image. You can create ELF, uimage and raw images, chose
whichever one you need for your target's boot loader.

    $ obj/l4/builddir
    $ make uimage PLATFORM_TYPE=rv_vexpress

This will present you a menu of selectable setups and will finally
build the image. You can avoid some typing by using shortcuts:

    $ make uimage E=hello PT=rv_vexpress

The built image can be found in the `images` sub-directory, e.g. as
`images/bootstrap_hello.uimage`.

Use that uimage file to load it on the target using u-boot.


Setup Configuration, and more
-----------------------------

The configuration file to configure the contents of images and generally
the entries to boot is

    src/l4/conf/modules.list

It contains `entry` sections with modules for each entries listed.
When using non-absolute paths, the image building will you the
`MODULE_SEARCH_PATH` to find those files. You can also use absolute paths.

The Makeconf.boot file is a `make` file, allowing for individual
configuration according to your needs. You may use available variables such
as `PLATFORM_TYPE`, `BUILD_ARCH`, and `QEMU_OPTIONS` to construct
configurations as required by different targets and architectures.

The Makeconf.boot file can also be stored in a build directory under the
`conf/` sub-directory.



Adding your own code
====================

Your own code should be placed outside the snapshot directory. This allows
that the snapshot can be replaced with a more recent version without
needing to take care about your own files and directories.

Software components are usually put into so-called packages, and each
package has a structure like this:

    pkgname/
            doc/               - Documentation for the package
            include/           - Public headers for the package
            lib/               - Library code
              src/
            server/            - Program code
              src/

This is just a recommended structure, it is not required to be like that.
What is built is defined in the Makefiles in each directory.

A typical Makefile looks like this:

    PKGDIR  ?= .
    L4DIR   ?= path/to/your/l4dir

    # Statements specific to the used role

    include $(L4DIR)/mk/<role>.mk

Role might be:
* `subdir`:  Descent to further subdirectories
* `lib`:     Build a library
* `prog`:    Build a program
* `include`: Process include files

The directory `l4/mk/tmpl` contains a template package directory layout
structure and shows how a package might look like. It also contains
examples on what to do in the Makefiles.

A very basic example might go like this:

    $ mkdir /tmp/myfirstpkg
    $ cd /tmp/myfirstpkg
    $ editor Makefile
    $ cat Makefile
    PKGDIR  ?= .
    L4DIR   ?= /path/to/snapshot/src/l4

    TARGET          = myfirstprogram
    SRC_C           = main.c
    
    include $(L4DIR)/mk/prog.mk
    $ editor main.c
    $ cat main.c
    #include <stdio.h>
    
    int main(void)
    {
      printf("Hello!\n");
      return 0;
    }
    $ make O=/path/to/snapshot/obj/l4/arm-rv-arm9
    ...
    $ ls /path/to/snapshot/obj/l4/arm-rv-arm9/bin/arm_rv/l4f/myfirstprogram
    /path/to/snapshot/obj/l4/arm-rv-arm9/bin/arm_rv/l4f/myfirstprogram
    $


Tips and tricks
---------------

If you're just building for one build directory you can do the
following to avoid the `O=...` argument on every make call.

Put `O=/path/to/the/build-dir` into L4DIR/Makeconf.local

Also, you can just issue 'make' in the build directories directly.


Setup for multiple packages
---------------------------

Create a directory structure like this:

    dir/
    dir/pkg1
    dir/pkg2
    dir/pkg3

Put this Makefile into dir/Makefile:

    PKGDIR  = .
    L4DIR   ?= /path/to/your/l4dir/l4
    
    TARGET = $(wildcard [a-zA-Z]*)
    
    include $(L4DIR)/mk/subdir.mk

This will build all sub packages from within this directory. Make sure
to define L4DIR properly in every Makefile in the packages (or
alternatively, include a file which defines it, but this file has to be
absolute as well). 
In the package directories you can have the normal Makefiles as in
`l4/pkg/pkgname`.

