#[macro_export]
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
#[macro_export]
macro_rules! unroll_types {
    (() -> $ret:ty) => {
        $crate::ipc::types::Return<$ret>
    };

    (($last:ty) -> $ret:ty) => {
        $crate::ipc::types::Sender<$last, $crate::ipc::types::Return<$ret>>
    };

    (($head:ty, $($tail:ty),*) -> $ret:ty) => {
        $crate::ipc::types::Sender<$head, unroll_types!(($($tail),*) -> $ret)>
    }
}

#[macro_export]
macro_rules! rpc_func {
    ($opcode:expr => $name:ident() -> $ret:ty) => {
        #[allow(non_snake_case)]
        pub struct $name($crate::ipc::types::Return<$ret>);
        rpc_func_impl!($name, i8, $opcode);
    };

    ($opcode:expr => $name:ident($type:ty) -> $ret:ty) => {
        #[allow(non_snake_case)]
        pub struct $name($crate::ipc::types::Sender<$type,
                $crate::ipc::types::Return<$ret>>);
        rpc_func_impl!($name, i8, $opcode);
    };

    ($opcode:expr => $name:ident($head:ty, $($tail:ty),*) -> $ret:ty) => {
        #[allow(non_snake_case)]
        pub struct $name($crate::ipc::types::Sender<$head,
                         unroll_types!(($($tail),*) -> $ret)>);
        rpc_func_impl!($name, i8, $opcode);
    }
}

#[macro_export]
macro_rules! rpc_func_impl {
    ($name:ident, $type:ty, $opcode:expr) => {
        impl $crate::ipc::types::HasOpCode for $name {
            type OpType = $type;
            const OP_CODE: Self::OpType = $opcode;
        }

        impl $name {
            #[inline]
            #[allow(unused_function)]
            pub fn new() -> $name {
                $name($crate::ipc::types::Sender(
                        ::core::marker::PhantomData))
            }
        }
    }
}

#[macro_export]
macro_rules! derive_functors {
    ($count:expr;) => ();

    ($count:expr; $name:ident($($types:ty),*) -> $ret:ty) => { // se below
        rpc_func!($count => $name($($types),*) -> $ret);
    };

    ($count:expr;
            $name:ident($($types:ty),*) -> $ret1:ty; // head of function list
                $($more:ident($($othertypes:ty),*) -> $ret2:ty);* // ← tail
    ) => { // return type
        rpc_func!($count => $name($($types),*) -> $ret1);
        derive_functors!($count + 1; $($more($($othertypes),*) -> $ret2);*);
    }
}

/// Extract the opcode from a list of function names
///
/// The basic assumption is that all functions share the same opcode type in an IPC interface.
/// Within a macro, it is impossible to separate the first element from a type list, hence this
/// helper macro gets the head and yields the opcode type.
#[macro_export]
macro_rules! opcode {
    ($iface:ident; $name:ident) => {
        <$iface::$name as $crate::ipc::types::HasOpCode>::OpType
    };
    ($iface:ident; $name:ident, $($other_names:ident),*) => {
        <$iface::$name as $crate::ipc::types::HasOpCode>::OpType
    }
}

#[macro_export]
macro_rules! derive_ipc_struct {
    (iface $traitname:ident($iface:ident):
     $($name:ident($($argname:ident: $type:ty),*) -> $return:ty;)*
    ) => {
        struct $traitname<T: $crate::cap::CapKind> {
            cap: $crate::cap::Cap<T>,
            utcb: $crate::utcb::Utcb,
        }

        impl $traitname<$crate::ipc::types::Client> {
            $(
                pub fn $name(&mut self, $($argname: $type),*)
                            -> $crate::error::Result<$return> {
                    let mut mr = self.utcb.mr();
                    // write opcode
                    unsafe {
                        mr.write(<$iface::$name as $crate::ipc::types::HasOpCode>
                                 ::OP_CODE)?;
                    }
                    write_msg!($iface::$name, mr, $($argname),*)?;
                    // ToDo: first param is protocol number, interface should define it, c++?`
                    // ToDo: third parameter is buffer registers, missing
                    // ToDo: flags aren't passed yet, C++ does it
                    let tag = $crate::ipc::MsgTag::new(0, mr.words(), 0, 0);
                    let restag = $crate::ipc::MsgTag::from(unsafe {
                            ::l4_sys::l4_ipc_call(self.cap.raw(),
                                    ::l4_sys::l4_utcb(), tag.raw(),
                                    ::l4_sys::timeout_never())
                        }).result()?;
                    // ToDo: return value extraction is purely guessed; ToDo: use restag
                    mr.reset();
                    // return () if "empty" return value, val otherwise
                    unsafe {
                        <$return as $crate::utcb::Serialisable>::read(
                                &mut mr)
                    }
                }
            )*
        }

        impl<T: $iface::$traitname> $traitname
                <$crate::ipc::types::Server<T>> {
            // ToDo: docstring + provide _u version which also takes utcb; think of better way to
            // create or receive capability
            pub fn from_impl(cap: $crate::cap::CapIdx, user_impl: T)
                    -> $traitname<$crate::ipc::types::Server<T>> {
                $traitname {
                    cap: $crate::cap::Cap::new(cap,
                            $crate::ipc::types::Server { user_impl }),
                    utcb: $crate::utcb::Utcb::current(),
                }
            }
        }

        impl<T: $iface::$traitname> $crate::ipc::types::Dispatch
                for $traitname<$crate::ipc::types::Server<T>> {
            fn dispatch(&mut self) -> $crate::error::Result<()> {
                let mut msg_mr = self.utcb.mr();
                let opcode: i8 = unsafe {
                        msg_mr.read::<opcode!($iface; $($name),*)>()?
                };
                $(
                    if opcode == <$iface::$name as $crate::ipc::types::HasOpCode>
                                ::OP_CODE {
                        // ToDo: use result
                        unsafe {
                            // ToDo: use result -> use for reply
                            (**self.cap).$name(
                                    $(msg_mr.read::<$type>()?),*
                                );
                            return Ok(());
                        }
                    }
                )*
                Err($crate::error::Error::InvalidArg("unknown opcode \
                        received", Some(opcode as isize)))
            }
        }
    }
}

#[macro_export]
macro_rules! iface {
    (mod $iface_name:ident;
     trait $traitname:ident {
        $(
            fn $name:ident(&mut self, $($argname:ident: $argtype:ty),*) -> $ret:ty;
        )*
    }) => {
        mod $iface_name {
            pub trait $traitname {
            $(
                fn $name(&mut self, $(argname: $argtype),*) -> $ret;
            )*
            }

            derive_functors!(0; $($name($($argtype),*) -> $ret);*);
        }
        derive_ipc_struct!(iface $traitname($iface_name):
            $($name($($argname: $argtype),*) -> $ret;)*);
    };

    (mod $iface_name:ident;
     trait $traitname:ident {
        $(
            fn $name:ident($($argname:ident: $argtype:ty),*);
        )*
    }) => {
        iface! {
            mod $iface_name;
            trait $traitname {
                $(
                    fn $name(&mut self, $($argname: $argtype),*) -> ();
                )*
            }
        }
    }
}

/*
iface! {
    mod echoserver;
    trait EchoServer {
        fn do_something(&mut self, i: i32) -> u8;
        fn do_something_else(&mut self, u: u64) -> ();
    }
}
*/

/*
mod echoserver {
    use super::super::types::*; // need to do it here manually
    use core::marker::PhantomData;

    pub trait EchoServer {
        fn do_something(&mut self, i: i32, a: i64) -> i32;
        fn do_something_else(&mut self, u: u64) -> u64;
    }

    derive_functors!(0; do_something(i32, i64) -> i32; do_something_else(u64) -> u64);
}

derive_ipc_struct! {
    iface EchoServer(echoserver):
    do_something(i: i32, b: i64) -> ();
    do_something_else(u: u64) -> u8;
}
*/
