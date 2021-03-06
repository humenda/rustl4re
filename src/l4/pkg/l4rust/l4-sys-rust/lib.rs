#![no_std]

extern crate libc;

#[macro_use]
pub mod helpers;
mod c_api;
mod cap;
pub mod consts;
mod factory;
mod ipc_basic;
mod ipc_ext;
mod platform;
mod task;

/// expose public C API
pub use crate::cap::*;
pub use crate::c_api::*;
pub use crate::factory::*;
pub use crate::ipc_ext::*;
pub use crate::ipc_basic::*;
pub use crate::platform::*;
pub use crate::task::*;

const L4_PAGEMASKU: l4_addr_t = L4_PAGEMASK as l4_addr_t;

#[inline]
pub fn trunc_page(address: l4_addr_t) -> l4_addr_t {
    address & L4_PAGEMASKU
}

/// Round address up to the next page.
///
/// The given address is rounded up to the next minimal page boundary. On most architectures this is a 4k
/// page. Check `L4_PAGESIZE` for the minimal page size.
#[inline]
pub fn round_page(address: usize) -> l4_addr_t {
    ((address + L4_PAGESIZE as usize - 1usize) & (L4_PAGEMASK as usize)) as l4_addr_t
}
