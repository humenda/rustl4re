#![no_std]

extern crate libc;

#[macro_use]
pub mod helpers;
mod cap;
mod c_api;
pub mod consts;
mod factory;
mod ipc_basic;
pub mod ipc_ext;
mod platform;
mod task;

/// expose public C API
pub use cap::*;
pub use c_api::*;
pub use factory::*;
pub use ipc_ext::*;
pub use ipc_basic::*;
pub use platform::*;
pub use task::*;

const L4_PAGESIZEU: usize = L4_PAGESIZE as usize;
const L4_PAGEMASKU: l4_addr_t = L4_PAGEMASK as l4_addr_t;
const L4_PAGEMASKUSIZE: usize = L4_PAGEMASK as usize;

#[inline]
pub fn l4_trunc_page(address: l4_addr_t) -> l4_addr_t {
    address & L4_PAGEMASKU
}

/// Round address up to the next page.
///
/// The given address is rounded up to the next minimal page boundary. On most architectures this is a 4k
/// page. Check `L4_PAGESIZE` for the minimal page size.
#[inline]
pub fn l4_round_page(address: usize) -> l4_addr_t {
    ((address + L4_PAGESIZE as usize - 1usize) & (L4_PAGEMASK as usize)) as l4_addr_t
}

/// the function above makes interfacing with l4 C code easy, that one below is more appropriate
/// (and potentially more efficient) for Rust code
#[inline]
pub fn l4_trunc_page_u(address: usize) -> usize {
    address & L4_PAGEMASKUSIZE
}

