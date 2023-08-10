//! L4Re interface crate
//!
//! Reimplemented methods
//#![no_std]
#![feature(associated_type_defaults)]

extern crate core as _core;
extern crate l4;
extern crate l4_derive;
extern crate l4_sys;
extern crate libc;

mod cap;
pub mod env;
pub mod mem;
pub mod sys;

pub use cap::OwnedCap;
