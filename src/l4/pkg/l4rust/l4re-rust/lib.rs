//! L4Re interface crate
//!
//! Reimplemented methods
#![no_std]

extern crate core as _core;
extern crate l4;
extern crate l4_derive;
extern crate libc;
extern crate l4_sys;

pub mod env;
mod cap;
//pub mod mem;
pub mod sys;

pub use cap::OwnedCap;
