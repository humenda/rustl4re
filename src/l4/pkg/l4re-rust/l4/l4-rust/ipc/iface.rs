use core::marker::PhantomData;

use super::super::{
    error::Result,
    utcb::{Msg, Serialisable}
};

/// compile-time specification of an opcode for a type
trait HasOpCode {
    type OpType;
    const OP_CODE: Self::OpType;
}

trait Dispatch {
    fn dispatch(message: Msg) -> Result<()>;
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
macro_rules! write_msg {
    ($func_representation:ty, $msg:expr, $(arg:expr),*) => {
        {
            let instance = $func_representation { PhantomData };
            $(
                let instance = instance.write($msg, $arg);
            )*
        }
    }
}


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

macro_rules! iface {
    (mod $iface_name:ident {
        $(fn $name:ident($($argname:ident: $argtype:ty),*);)*
    }) => {
        mod $iface_name {
            trait Spec {
            $(
                fn $name(&mut self, $(argname: $argtype),*);
             )*
            }

            mod functors {
                use super::super::*;

                derive_functors!(0; $($name($($argtype),*)),*);
            }
        }
    }
}


iface! {
    mod echoserver {
        fn do_something(i: i32);
        fn do_something_else(u: u64);
    }
}

