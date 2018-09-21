use core::marker::PhantomData;

use super::super::{
    cap::{Cap, CapIdx, CapKind},
    error::{Error, Result},
    utcb::Utcb
};
use ipc::{call, types::*, MsgTag};

macro_rules! write_msg {
    ($funcstruct:ty, $msg_mr:expr, $($arg:expr),*) => {
        {
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
            pub fn new() -> $name {
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

/// Extract the opcode from a list of function names
///
/// The basic assumption is that all functions share the same opcode type in an IPC interface.
/// Within a macro, it is impossible to separate the first element from a type list, hence this
/// helper macro gets the head and yields the opcode type.
macro_rules! opcode {
    ($name:ident, $($other_names:ident),*) => {
        <__iface::$name as HasOpCode>::OpType
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
                    let mut mr = self.utcb.mr();
                    // write opcode
                    unsafe {
                        mr.write(__iface::$name::OP_CODE)?;
                    }
                    write_msg!(__iface::$name, mr, $($argname),*)?;
                    // ToDo: first param is protocol number, interface should define it, c++?`
                    // ToDo: third parameter is buffer registers, missing
                    // ToDo: flags aren't passed yet, C++ does it
                    let tag = MsgTag::new(0, mr.words(), 0, 0);
                    // ToDo: fill in args for syscall
                    //let msgtag = call(dest: &Cap<T>, &mut Utcb::current,
                    //                          tag, ::l4_sys::timeout_never())
                    //    .result()?;
                    // ToDo: extract return value
                    Ok(())
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
                let opcode: i8 = unsafe {
                        msg_mr.read::<opcode!($($name),*)>()?
                };
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

