// ToDo: things to be stabilised in Rust:
#![feature(align_offset)]
#![feature(associated_type_defaults)]

#![no_std]
#[cfg(feature = "std")]
extern crate std;

#[cfg(not(without_std))]
pub extern crate core as _core;

extern crate l4_sys;
#[macro_use]
extern crate bitflags;
#[macro_use]
extern crate num_derive;
extern crate num_traits;

// apart from macro_use, eep this alphabetical
#[macro_use]
pub mod types;
#[macro_use]
pub mod error;
#[macro_use]
pub mod utcb;
pub mod cap;
pub mod ipc;
pub mod task;

pub use crate::error::{Error, Result};
pub use crate::utcb::*;

pub mod sys {
    pub use l4_sys::*;
}
