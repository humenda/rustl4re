//! IPC Interface Types

use core::{
    ops::{Deref, DerefMut},
    marker::PhantomData
};

use super::super::{
    error::Result,
    cap::{CapKind, CapIdx},
    utcb::Serialisable
};

/// compile-time specification of an opcode for a type
///
/// The operand code (opcode) is used to identify the message received and
/// defines to which server-provided handler it is dispatched.
pub trait HasOpCode {
    type OpType;
    const OP_CODE: Self::OpType;
}

/// Opcode-based Function Dispatch
/// 
/// A type implementing this trait is able to read an opcode from the message registers and
/// dispatch to the function implementation accordingly. This trait is independent from the actual
/// method of function retrieval and just specifies that the type is able to dispatch.
pub trait Dispatch {
    /// Dispatch message from client to the user-defined server implementation
    ///
    /// Using the operation code (opcode) for each IPC call  defined in an interface, the arguments
    /// are read from the UTCB and fed into the user-supplied server implementation. Please note
    /// that the opcode might have been defined automatically, if no arguments were supplied.
    /// The result of the operation will be written back to the UTCB, errors are propagated upward.
    fn dispatch(&mut self) -> Result<()>;
}

/// Define a function in a type and allow the derivation of the opposite type
///
/// This allows the macro rules to define the type for a sender and derive the receiver part
/// automatically.
trait HasOpposite {
    type Opposite;
}

/// End Of Type List
///
/// RPC functions are represented as a type system list of function arguments. This type defines
/// the end node.
pub struct End;

impl HasOpposite for End {
    type Opposite = End;
}

/// Intermediate Function List Node
///
/// RPC functions are represented as a linked list of types, each function argument being a node. A
/// `Sender` consists of the current type and a type reference to the next type. `Sender`
/// identifies that this function parameter list defines the client side. Note that the server side
/// can be automatically be derived, see the [HasDual trait](trait.HasDual.html).
pub struct Sender<ValueType, NextType>(
        pub PhantomData<(ValueType, NextType)>);

impl<ValueType: Serialisable, Next: HasOpposite> HasOpposite for Sender<ValueType, Next> {
    type Opposite = Receiver<ValueType, Next::Opposite>;
}


/// Intermediate Function List Node
///
/// Please see [Sender<Val, Next>](struct.Sender.html) for details.
pub struct Receiver<ValueType, Next>(
        pub PhantomData<(ValueType, Next)>);

impl<Val: Serialisable, Next: HasOpposite> HasOpposite for Receiver<Val, Next> {
    type Opposite = Sender<Val, Next::Opposite>;
}

/// Empty type to symbolise client-side access to the interface.
pub struct Client(CapIdx);

impl CapKind for Client { }

/// Server-side marker For An IPC Interface
///
/// This marker switches an interface into server mode.
/// The user has to supply an implementation of server-side functions. This is usually by
/// implementing a trait like this:
// ToDo: example implementation of a user_impl
pub struct Server<T> {
    pub user_impl: T,
}

impl<T> CapKind for Server<T> { }

impl<T> Deref for Server<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.user_impl
    }
}


impl<T> DerefMut for Server<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.user_impl
    }
}

