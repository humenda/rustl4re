//! L4Re interface crate
//!
//! Reimplemented methods
#![no_std]
#![feature(associated_type_defaults)]

extern crate core as _core;
extern crate l4;
extern crate l4_derive;

mod cap;
pub mod env;
pub mod mem;
pub mod sys;
pub mod factory;
pub mod io;

pub use cap::OwnedCap;
