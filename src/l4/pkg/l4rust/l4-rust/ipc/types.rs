//! IPC Interface Types

use _core::{
    default::Default,
    ops::{Deref, DerefMut},
    marker::PhantomData
};

use super::super::{
    error::Result,
    ipc::MsgTag,
    utcb::{Msg, Serialisable}
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

/// Define a function in a type and allow the derivation of the dual type
///
/// This allows the macro rules to define the type for a sender and derive the receiver part
/// automatically.
pub trait HasDual {
    type Dual;
}

pub trait ReadReturn<T> {
    fn read(mr: &mut Msg) -> T;
}

/// private marker for ()
/// Return Type Of Function Struct
///
/// RPC functions are represented as a type system list of function arguments. This type defines
/// the return type and serves as the list end.
#[derive(Default)]
pub struct Return<T>(PhantomData<T>);

impl<T: Serialisable> HasDual for Return<T> {
    type Dual = Return<T>;
}


/// Intermediate Function List Node
///
/// RPC functions are represented as a linked list of types, each function argument being a node. A
/// `Sender` consists of the current type and a type reference to the next type. `Sender`
/// identifies that this function parameter list defines the client side. Note that the server side
/// can be automatically be derived, see the [HasDual trait](trait.HasDual.html).
#[derive(Default)]
pub struct Sender<ValueType, NextType>(
        pub PhantomData<(ValueType, NextType)>);

impl<ValueType: Serialisable, Next: HasDual> HasDual for Sender<ValueType, Next> {
    type Dual = Receiver<ValueType, Next::Dual>;
}


impl<ValTy, Next> Sender<ValTy, Next> 
        where ValTy: Serialisable, Next: HasDual + Default {
    pub unsafe fn read(self, msg: &mut Msg) -> (Next, Result<ValTy>) {
        (Next::default(), msg.read::<ValTy>())
    }
}

/// Intermediate Function List Node For Receiver Site
///
/// Please see [Sender<Val, Next>](struct.Sender.html) for details.
#[derive(Default)]
pub struct Receiver<ValueType, Next>(
        pub PhantomData<(ValueType, Next)>);

impl<Val: Serialisable, Next: HasDual> HasDual for Receiver<Val, Next> {
    type Dual = Sender<Val, Next::Dual>;
}

impl<ValTy, Next> Receiver<ValTy, Next>
        where ValTy: Serialisable, Next: HasDual + Default {
    pub unsafe fn read(self, msg: &mut Msg) -> (Next, Result<ValTy>) {
        (Next::default(), msg.read::<ValTy>())
    }
}

/// Empty type to symbolise client-side access to the interface.
pub struct Client;

/// Server-side marker For An IPC Interface
///
/// This marker switches an interface into server mode.
/// The user has to supply an implementation of server-side functions. This is usually by
/// implementing a trait like this:
// ToDo: example implementation of a user_impl
pub struct Server<T>(T);

impl<T> Deref for Server<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> DerefMut for Server<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<T> Server<T> {
    pub fn new(uimpl: T) -> Server<T> {
        Server { 0: uimpl }
    }
}
