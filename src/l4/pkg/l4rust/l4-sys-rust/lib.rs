#![no_std]

#[macro_use]
mod ipc_ext;
mod c_api;
mod cap;
pub mod consts;
mod factory;
pub mod helpers;
mod ipc_basic;
mod platform;
mod scheduler;
mod task;

pub use crate::c_api::*;
/// expose public C API
pub use crate::cap::*;
pub use crate::factory::*;
pub use crate::ipc_basic::*;
pub use crate::ipc_ext::*;
pub use crate::platform::*;
pub use crate::scheduler::*;
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

pub mod util {
    #[cfg(target_arch = "x86_64")]
    pub fn rdtscp() -> u64 {
        unsafe {
            let mut _val = 0;
            core::arch::x86_64::__rdtscp(&mut _val)
        }
    }
}

#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
pub type L4Umword = u64;
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
pub type L4Mword = i64;

#[cfg(any(target_arch = "x86", target_arch = "aarch32"))]
pub type L4Umword = u32;
#[cfg(any(target_arch = "x86", target_arch = "aarch32"))]
pub type L4Mword = i32;

