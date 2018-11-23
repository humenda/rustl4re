#![no_std]
#![feature(align_offset)]
#![feature(alloc)]
// ToDo: ^ stable Rust

extern crate alloc;

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
pub mod server;
pub mod task;

pub use error::{Error, Result};
pub use utcb::*;


