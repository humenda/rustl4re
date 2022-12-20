#![no_std]
#[cfg(feature = "std")]
extern crate std;

#[macro_use]
extern crate bitflags;
#[macro_use]
extern crate num_derive;
extern crate num_traits;

// apart from macro_use, keep this alphabetical
#[macro_use]
pub mod types;
#[macro_use]
pub mod error;
#[macro_use]
pub mod utcb;
pub mod cap;
pub mod ipc;
#[cfg(not(feature = "std"))]
#[macro_use]
pub mod nostd_helper;
pub mod task;

pub use crate::error::{Error, Result};
pub use crate::utcb::*;

pub mod sys {
    pub use l4_sys::*;
}
