// ToDo: things to be stabilised in Rust:
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

// a few re-exports
pub use sys::{round_page, trunc_page};

////////////////////////////////////////////////////////////////////////////////
// Bench marking

#[derive(Clone, Copy)]
pub struct ClientCall {
    // begin of serialisation of method parameters (after opcode)
    pub arg_serialisation_start: u64,
    // end of it
    pub arg_serialisation_end: u64,
    // begin of IPC call (message tag was created between arg srl end and call start)
    pub ipc_call_start: u64,
    // after IPC call, return value is going to be read; includes error checking
    pub return_val_start: u64, // doesn't have end, ended by call itself
    // ToDo TMP
    pub return_val_end: u64,
}

impl ClientCall { pub const fn new() -> Self { ClientCall {
    arg_serialisation_start: 0, arg_serialisation_end: 0, ipc_call_start: 0,
    return_val_start: 0, return_val_end: 0
}}}

#[derive(Clone, Copy)]
pub struct ServerDispatch {
    /// label received from kernel, wild casting starts
    pub loop_dispatch: u64,
    /// op_dispatch auto-implemented by iface! macro starts, about to check
    /// message tag and read opcode
    pub iface_dispatch: u64,
    // opcode is matched to dispatch to appropriate function, just before
    // function is executed
    pub opcode_dispatch: u64,
    // start of user impl
    pub exc_user_impl: u64,
    // after user-implemented function handler
    pub retval_serialisation_start: u64,
    pub result_returned: u64,
    // start of handling service result in server loop handler
    //pub hook_start: u64,
    /// end of hook invocation, end of IPC server part
    pub hook_end: u64,
}
impl ServerDispatch { pub const fn new() -> Self { ServerDispatch {
    loop_dispatch: 0, iface_dispatch: 0, opcode_dispatch: 0, exc_user_impl: 0,
    retval_serialisation_start: 0, result_returned: 0, //hook_start: 0,
    hook_end: 0,
}}}

#[cfg(bench_serialisation)]
pub static mut CLIENT_MEASUREMENTS: Measurements<ClientCall> = Measurements {
    buf: [ClientCall::new(); MEASURE_RUNS],
    index: 0usize,
};

// pointer for server-side measurements; normally redirected to a shared memory region, but has a
// backup for safety reasons
#[cfg(bench_serialisation)]
pub static mut SERVER_MEASUREMENTS: *mut Measurements<ServerDispatch> =
        0 as *mut _;

pub const MEASURE_RUNS: usize = 10000;

#[cfg(bench_serialisation)]
pub struct Measurements<T> {
    buf: [T; MEASURE_RUNS],
    pub index: usize,
}

#[cfg(bench_serialisation)]
unsafe impl<T: core::marker::Sized> core::marker::Sync for Measurements<T> { }

#[cfg(bench_serialisation)]
impl<T> Measurements<T> {
    #[inline]
    pub fn last(&mut self) -> &mut T {
        &mut self.buf[self.index - 1]
    }
    #[inline]
    pub fn next<'a>(&'a mut self) -> &'a mut T {
        let val = &mut self.buf[self.index];
        self.index += 1;
        val
    }

    /// Return a slice of all initialised values
    pub fn as_slice(&self) -> &[T] {
        &self.buf[0..self.index]
    }

    /// Access to whole backing array, not just the written part
    pub unsafe fn as_mut_slice(&mut self) -> &mut [T] {
        &mut self.buf[..]
    }
}
