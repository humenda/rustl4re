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
    // begin of auto-derived IPC call
    pub call_start: i64,
    // begin of serialisation of method parameters (after opcode)
    pub arg_serialisation_start: i64,
    // end of it
    pub arg_serialisation_end: i64,
    // begin of IPC call (message tag was created between arg srl end and call start)
    pub ipc_call_start: i64,
    // after IPC call, return value is going to be read; includes error checking
    pub return_val_start: i64, // doesn't have end, ended by call itself
    // end of the auto-generated call, i.e. after reading the return value
    pub call_end: i64
}

impl ClientCall { pub const fn new() -> Self { ClientCall {
    call_start: 0, arg_serialisation_start: 0, arg_serialisation_end: 0,
    ipc_call_start: 0, return_val_start: 0, call_end: 0
}}}

#[derive(Clone, Copy)]
pub struct ServerDispatch {
    /// label received from kernel, wild casting starts
    pub loop_dispatch: i64,
    /// op_dispatch auto-implemented by iface! macro starts
    pub srv_dispatch: i64,
    // opcode is matched to dispatch to appropriate function and function is
    // executed
    pub opcode_dispatch: i64,
    // after user-implemented function handler
    pub retval_serialisation_start: i64,
    pub result_returned: i64,
    // start of handling service result in server loop handler
    pub hook_start: i64,
    /// end of hook invocation, dispatch result as integer
    pub hook_end: i64,
}
impl ServerDispatch { pub const fn new() -> Self { ServerDispatch {
    loop_dispatch: 0, srv_dispatch: 0, opcode_dispatch: 0,
    retval_serialisation_start: 0, result_returned: 0, hook_start: 0, hook_end: 0,
}}}

#[cfg(bench_serialisation)]
pub static mut CLIENT_MEASUREMENTS: Measurements<ClientCall> = Measurements {
    buf: [ClientCall::new(); MEASURE_RUNS],
    index: 0usize,
};

// pointer for server-side measurements; normally redirected to a shared memory region, but has a
// backup for safety reasons
//#[cfg(bench_serialisation)]
//pub static mut SERVER_MEASUREMENTS_BACKUP: Measurements<ServerDispatch> = Measurements {
//    buf: [ServerDispatch::new(); MEASURE_RUNS],
//    index: 0usize,
//};
#[cfg(bench_serialisation)]
pub static mut SERVER_MEASUREMENTS: *mut Measurements<ServerDispatch> =
        0 as *mut _;

pub const MEASURE_RUNS: usize = 100000;

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
    pub fn push(&mut self, s: T) {
        self.buf[self.index] = s;
        self.index += 1;
    }

    pub fn as_slice(&self) -> &[T] {
        &self.buf[0..self.index]
    }
}
