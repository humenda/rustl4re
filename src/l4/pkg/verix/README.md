# `verix`
This is a formally verified driver for the Intel 82599 NIC (whose drivers
are commonly called `ix` or `ixgbe`). It consists these packages:
1. `verix`, the actual implementation of the driver itself and the thing that
    actually runs on L4 in the end
2. `pc-hal`, a hardware abstraction layer library in the spirit of [`embedded-hal`](https://github.com/rust-embedded/embedded-hal)
3. `pc-hal-l4`, an implementation of `pc-hal` based on L4re services to access hardware and memory mappings
4. `pc-hal-util`, a utility library for `pc-hal` that provides:
   - typed access to the PCI config space
   - a macro framework to define MMIO in the style of [`svd2rust`](https://github.com/rust-embedded/svd2rust)
   - common PCI functions such as mapping a BAR, finding capabilities etc.
5. `mix`, the model of `ix` it contains a model of the subset of the Intel 82599 that is actually used
    by `verix` together with [`kani`](https://github.com/model-checking/kani) based verification cases
