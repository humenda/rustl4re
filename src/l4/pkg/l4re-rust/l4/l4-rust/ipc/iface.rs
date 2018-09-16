use core::marker::PhantomData;
use core::ops::{Deref, DerefMut};

use super::super::{
    cap::{Cap, CapIdx, CapKind, CapInit},
    error::{Error, Result},
    utcb::{Msg, Serialisable, Utcb}
};

/// compile-time specification of an opcode for a type
trait HasOpCode {
    type OpType;
    const OP_CODE: Self::OpType;
}

trait Dispatch {
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

pub struct End;

impl HasOpposite for End {
    type Opposite = End;
}

pub struct Sender<ValueType, NextType>(
        PhantomData<(ValueType, NextType)>);

impl<ValueType: Serialisable, Next: HasOpposite> HasOpposite for Sender<ValueType, Next> {
    type Opposite = Receiver<ValueType, Next::Opposite>;
}

// we may not constraint Next with a trait requirement, because this will prefvent the compiler
// from auto-deriving children types from the Next element
pub struct Receiver<ValueType, Next>(
        PhantomData<(ValueType, Next)>);

impl<Val: Serialisable, Next: HasOpposite> HasOpposite for Receiver<Val, Next> {
    type Opposite = Sender<Val, Next::Opposite>;
}

struct Client;

impl CapKind for Client { }

pub struct Server<T> {
    user_impl: T,
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
macro_rules! write_msg {
    ($funcstruct:ty, $msg_mr:expr, $($arg:expr),*) => {
        {
            type Foo = $funcstruct;
            let instance = Foo::new();
            unsafe {
                $(
                    $msg_mr.write($arg)?;
                 )*
            }
            Ok(())
        }
    }
}

/// Read IPC arguments from the message registers and create a comma-separate argument list to be
/// inserted into a function call:
///
///     { read_instruct() }, { read_instruct()  }, …

macro_rules! unroll_types {
    () => {
        End
    };

    ($last:ty) => {
        Sender<$last, End>
    };

    ($head:ty, $($tail:ty),*) => {
        Sender<$head, unroll_types!($($tail),*)>
    }
}

macro_rules! rpc_func {
    ($opcode:expr => $name:ident()) => {
        #[allow(non_snake_case)]
        pub struct $name(End);
        rpc_func_impl!($name, i8, $opcode);
    };

    ($opcode:expr => $name:ident($type:ty)) => {
        #[allow(non_snake_case)]
        pub struct $name(Sender<$type, End>);
        rpc_func_impl!($name, i8, $opcode);
    };

    ($opcode:expr => $name:ident($head:ty, $($tail:ty),*)) => {
        #[allow(non_snake_case)]
        pub struct $name(Sender<$head, unroll_types!($($tail),*)>);
        rpc_func_impl!($name, i8, $opcode);
    }
}

macro_rules! rpc_func_impl {
    ($name:ident, $type:ty, $opcode:expr) => {
        impl HasOpCode for $name {
            type OpType = $type;
            const OP_CODE: Self::OpType = $opcode;
        }

        impl $name {
            #[inline]
            fn new() -> $name {
                $name(Sender(PhantomData))
            }
        }
    }
}

macro_rules! unroll_mr {
    ($msg_instance:expr, $($type:ty),*) => {
        $({
            $msg_instance.read::<$type>()?
        }),*
    }
}

macro_rules! derive_functors {
    ($count:expr;) => ();

    ($count:expr; $name:ident($($types:ty),*)) => {
        rpc_func!($count => $name($($types),*));
    };

    ($count:expr; $name:ident($($types:ty),*), // head of function list
                $($more:ident($($othertypes:ty),*)),*) => { // ← tail of list
        rpc_func!($count => $name($($types),*));
        derive_functors!($count + 1; $($more($($othertypes),*)),*);
    }
}

macro_rules! derive_ipc_struct {
    (iface $iface:ident:
     $($name:ident($($argname:ident: $type:ty),*);)*
    ) => {
        struct $iface<T: CapKind> {
            cap: Cap<T>,
            utcb: Utcb,
        }

        impl $iface<Client> {
            $(
                pub fn $name(&mut self, $($argname: $type),*) -> Result<()> {
                    let mut msg = self.utcb.mr();
                    write_msg!(__iface::$name, msg, $($argname),*)
                }
            )*
        }

        impl<T: Spec> $iface<Server<T>> {
            // ToDo: docstring + provide _u version which also takes utcb; think of better way to
            // create or receive capability
            pub fn from_impl(cap: CapIdx, user_impl: T) -> $iface<Server<T>> {
                $iface {
                    cap: Cap::new(cap, Server { user_impl }),
                    utcb: Utcb::current(),
                }
            }
        }

        impl<T: Spec> Dispatch for $iface<Server<T>> {
            fn dispatch(&mut self) -> Result<()> {
                let mut msg_mr = self.utcb.mr();
                // ToDo: i8 as opcode assumed here, should be more dynamic
                let opcode: i8 = unsafe { msg_mr.read::<i8>()? };
                $(
                    if opcode == __iface::$name::OP_CODE {
                        // ToDo: use result
                        unsafe {
                            // ToDo: use result -> use for reply
                            (**self.cap).$name(
                                    $(msg_mr.read::<$type>()?),*
                                );
                        }
                        return Ok(());
                    }
                )*
                Err(Error::InvalidArg("unknown opcode received", Some(opcode as isize)))
            }
        }
    }
}

macro_rules! iface {
    (mod $iface_name:ident {
        $(fn $name:ident($($argname:ident: $argtype:ty),*);)*
    }) => {
        mod __iface {
            use super::*;
            trait Spec {
            $(
                fn $name(&mut self, $(argname: $argtype),*);
            )*
            }

            derive_functors!(0; $($name($($argtype),*)),*);
            derive_ipc_struct!(iface $iface_name:
                $($name($($argname: $argtype),*);)*);
        }
    }
}

iface! {
    mod echoserver {
        fn do_something(i: i32);
        fn do_something_else(u: u64);
    }
}

