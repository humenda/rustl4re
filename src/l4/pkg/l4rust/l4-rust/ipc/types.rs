//! IPC Interface Types

use super::super::{
    error::Result,
    ipc::MsgTag,
    utcb::UtcbMr
};
use l4_sys::l4_utcb_t;

/// IPC Dispatch Delegation Functionality
///
/// When implementing the server-side logic for an IPC service, the user needs
/// to implement all required IPC functions and register it with the server
/// loop. When a new message arrives, the user implementationneeds to dispatch
/// the message to the correct IPC implementation. This dispatch information is
/// auto-generated in the interface. Therefore this trait provides a default
/// implementation to delegate the dispatching to the auto-derived interface
/// definition, passing the actual arguments back to the user implementation,
/// once read from the UTCB.
pub trait Dispatch {
    fn dispatch(&mut self, tag: MsgTag, u: *mut l4_utcb_t) -> Result<MsgTag>;
}

/// Empty marker trait for server-side message dispatch
///
/// Types implementing this trait **must** make sure that their first element is
/// a function pointer pointing to the dispatch method of the
/// [Dispatch trait](trait.Dispatch.html); use `#[repr(C)]` for a guaranteed
/// struct member order.  
/// The function pointer must have the type
/// `fn(&mut self, MsgTag, *mut l4_utcb_t) -> Result<MsgTag>`.
pub unsafe trait Callable: Dispatch { }

pub trait Demand {
    const BUFFER_DEMAND: u32;
}

/// Define a function in a type and allow the derivation of the dual type
///
/// This allows the macro rules to define the type for a sender and derive the receiver part
/// automatically.
pub trait HasDual {
    type Dual;
}

pub trait ReadReturn<T> {
    fn read(mr: &mut UtcbMr) -> T;
}

