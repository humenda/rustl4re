# L4Re IO

IO is the management component that handles access to platform devices and
resources such as I/O memory, ports (on x86) and interrupts. It grants and
controls access to these resources for all other L4Re components.

This package includes the following components:

* io - main server and resource manager
* libvbus - IO client interface and definitions
* libio-direct - convenience functions for requesting hardware resources
                 directly from the kernel/sigma0
* libio-io - convenience functions for requesting hardware resources from IO

# Documentation

This package is part of the L4Re operating system. For documentation and
build instructions see the
[L4Re wiki](https://kernkonzept.com/L4Re/guides/l4re).

# Contributions

We welcome contributions. Please see our contributors guide on
[how to contribute](https://kernkonzept.com/L4Re/contributing/l4re).

# License

Detailed licensing and copyright information can be found in
the [LICENSE](LICENSE.spdx) file.
