#[macro_export]
macro_rules! write_msg {
    ($msg_mr:expr, $($arg:ident: $argty:ty),*) => {
        {
            unsafe {
                $(
                    <$argty as $crate::ipc::Serialiser>::write($arg, $msg_mr)?;
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
        $crate::ipc::types::Sender<$head, $crate::unroll_types!(($($tail),*) -> $ret)>
    }
}

#[macro_export]
macro_rules! rpc_func {
    ($opcode:expr => $name:ident() -> $ret:ty) => {
        #[allow(non_snake_case)]
        pub struct $name($crate::ipc::types::Return<$ret>);
        $crate::rpc_func_impl!($name, $crate::ipc::types::OpCode, $opcode);
    };

    ($opcode:expr => $name:ident($type:ty) -> $ret:ty) => {
        #[allow(non_snake_case)]
        pub struct $name($crate::ipc::types::Sender<$type,
                $crate::ipc::types::Return<$ret>>);
        $crate::rpc_func_impl!($name, $crate::ipc::types::OpCode, $opcode);
    };

    ($opcode:expr => $name:ident($head:ty, $($tail:ty),*) -> $ret:ty) => {
        #[allow(non_snake_case)]
        pub struct $name($crate::ipc::types::Sender<$head,
                         $crate::unroll_types!(($($tail),*) -> $ret)>);
        rpc_func_impl!($name, $crate::ipc::types::OpCode, $opcode);
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
            pub fn new() -> $name {
                $name($crate::ipc::types::Sender(
                        $crate::_core::marker::PhantomData))
            }
        }
    }
}

#[macro_export]
macro_rules! derive_functors {
    ($count:expr;) => ();

    ($count:expr; $name:ident($($types:ty),*) -> $ret:ty) => { // se below
        $crate::rpc_func!($count => $name($($types),*) -> $ret);
    };

    ($count:expr;
            $name:ident($($types:ty),*) -> $ret1:ty; // head of function list
                $($more:ident($($othertypes:ty),*) -> $ret2:ty);* // ← tail
    ) => { // return type
        $crate::rpc_func!($count => $name($($types),*) -> $ret1);
        $crate::derive_functors!($count + 1; $($more($($othertypes),*) -> $ret2);*);
    }
}

/// Extract the opcode from a list of function names
///
/// The basic assumption is that all functions share the same opcode type in an IPC interface.
/// Within a macro, it is impossible to separate the first element from a type list, hence this
/// helper macro gets the head and yields the opcode type.
#[macro_export]

macro_rules! derive_ipc_calls {
    ($proto:expr; $($opcode:expr =>
       fn $name:ident($($argname:ident: $type:ty),*) -> $return:ty;)*
    ) => {
        $(
            fn $name(&mut self $(, $argname: $type)*)
                        -> $crate::error::Result<$return> 
                            where Self: $crate::cap::Interface {
                use $crate::ipc::CapProvider;
                // ToDo: would re-allocate a capability each time; how to control number of
                // required slots
                // ToDo: This is client code, yet implemented in the trait, lacking access to
                // client state, e.g. the buffer allocator
                let mut caps = $crate::ipc::types::Bufferless { };
                let mut mr = $crate::utcb::Utcb::current().mr();
                // write opcode
                unsafe {
                    mr.write($opcode)?;
                }
                $crate::write_msg!(&mut mr, $($argname: $type),*)?;
                // get the protocol for the msg tag label
                let tag = $crate::ipc::MsgTag::new($proto, mr.words(),
                        mr.items(), 0);
                let _restag = $crate::ipc::MsgTag::from(unsafe {
                        ::l4_sys::l4_ipc_call(self.cap(),
                                ::l4_sys::l4_utcb(), tag.raw(),
                                ::l4_sys::timeout_never())
                    }).result()?;
                mr.reset(); // read again from start of registers, `caps` still untouched
                // return () if "empty" return value, val otherwise
                unsafe {
                    let mut cap_buf = caps.access_buffers();
                    <$return as $crate::ipc::Serialiser>::read(
                            &mut mr, &mut cap_buf)
                }
            }
        )*
    }
}

#[macro_export]
macro_rules! iface_back {
    // ideal iface declaration with op codes
    (
        trait $traitname:ident {
            const PROTOCOL_ID: i64 = $proto:tt;
            type OpType = $op_type:tt;
            $(
                $op_code:expr => fn $name:ident(&mut self
                        $(, $argname:ident: $argtype:ty)*) -> $ret:ty;
            )*
        }
    ) => {
        pub trait $traitname: $crate::cap::Interface
                    + $crate::ipc::CapProviderAccess {
            type OpType = $op_type;
            const PROTOCOL_ID: i64 = $proto;
            $crate::derive_ipc_calls!($proto; $($op_code =>
                    fn $name($($argname: $argtype),*) -> $ret;)*);

            // ToDo: document why op_dispatch implemented here
            fn op_dispatch(&mut self, tag: $crate::ipc::MsgTag,
                    mr: &mut $crate::utcb::UtcbMr,
                    bufs: &mut $crate::ipc::BufferAccess)
                    -> $crate::error::Result<$crate::ipc::MsgTag> {
                // uncover cheating clients
                if (tag.words() + tag.items() * $crate::utcb::WORDS_PER_ITEM)
                        > $crate::utcb::Consts::L4_UTCB_GENERIC_DATA_SIZE as usize {
                    return Err($crate::error::Error::Generic(
                            $crate::error::GenericErr::MsgTooLong))
                }
                // ToDo: check for correct number of words **AND** **important** correct number of
                // items
                if tag.label() != Self::PROTOCOL_ID {
                    return Err($crate::error::Error::Generic(
                            $crate::error::GenericErr::InvalidProt))
                }
                let opcode = unsafe { mr.read::<$op_type>()? };
                $( // iterate over functions and op codes
                if opcode == $op_code {
                    unsafe {
                        let user_ret = self.$name(
                            $(<$argtype as $crate::ipc::Serialiser>::
                                read(mr, bufs)?),*
                        )?;
                        mr.reset(); // reset MRs, not the bufs (not used for sending)
                        <$ret as $crate::ipc::Serialiser>::write(user_ret, mr)?;
                        // on return, label/protocol is 0 for success and a custom error code
                        // otherwise; negative means some IPC failure
                        return Ok($crate::ipc::MsgTag::new(0,
                                mr.words(), mr.items(), 0));
                    }
                }
                )* // ↓ incorrect op code
                Err($crate::error::Error::InvalidArg("unknown opcode \
                        received", Some(opcode as isize)))
            }
        }
    }
}

#[macro_export]
macro_rules! iface_enumerate {
    // match already enumerated interfaces and pass them directly through
    (
        trait $traitname:ident {
            const PROTOCOL_ID: i64 = $proto:tt;
            type OpType = $op_type:tt;
            $(
                $opcode:expr => fn $name:ident(&mut self $(, $argname:ident: $argtype:ty)*) -> $ret:ty;
            )*
        }
    ) => {
        $crate::iface_back! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                type OpType = $op_type;
                $(
                    $opcode => fn $name(&mut self $(, $argname: $argtype)*) -> $ret;
                )*
            }
        }
    };

    (// match unnumbered interface and start enumerating it
        trait $traitname:ident {
            const PROTOCOL_ID: i64 = $proto:tt;
            type OpType = $op_type:tt;
            $(
                fn $name:ident(&mut self
                        $(, $argname:ident: $argtype:ty)*) -> $ret:ty;
            )*
        }
     ) => {
        $crate::iface_enumerate! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                type OpType = $op_type;
                current_opcode = 0;
                {
                $(
                    fn $name(&mut self $(, $argname: $argtype)*) -> $ret;
                )*
                }
            }
        }
    };

    (// match next unnumbered function
        trait $traitname:ident {
            const PROTOCOL_ID: i64 = $proto:tt;
            type OpType = $op_type:tt;
            current_opcode = $count:expr;
            { fn $name:ident(&mut self $(, $argname:ident: $argtype:ty)*) -> $ret:ty;
                $($unmatched:tt)*
            }
            $($matched:tt)*
        }
     ) => {
        $crate::iface_enumerate! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                type OpType = $op_type;
                current_opcode = $count + 1;
                { $($unmatched)* }
                $($matched)*
                $count => fn $name(&mut self $(, $argname: $argtype)*) -> $ret;
            }
        }
    };

    (// all functions were numbered, call the ideal case (see first matcher)
        trait $traitname:ident {
            const PROTOCOL_ID: i64 = $proto:tt;
            type OpType = $op_type:tt;
            current_opcode = $count:expr;
            { }
            $($matched:tt)*
        }
    ) => {
        $crate::iface_back! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                type OpType = $op_type;
                $($matched)*
            }

        }
    };
}

#[macro_export]
macro_rules! iface {
    // entry point with return type
    (trait $traitname:ident {
        const PROTOCOL_ID: i64 = $proto:tt;
        $(
            fn $name:ident(&mut self, $($argname:ident: $argtype:ty),*) $(-> $ret:ty)?;
        )*
    }) => {
        $crate::iface! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                {
                    $(
                    fn $name($($argname: $argtype),*) $(-> $ret)?;
                    )*
                }
            }
        }
    };

    // matcher for next IPC function having no return type
    (trait $traitname:ident {
         const PROTOCOL_ID: i64 = $proto:tt;
         {
             fn $name:ident($($argname:ident: $argtype:ty),*);
             $($unexpanded:tt)*
         }
         $($expanded:tt)*
    }) => {
        $crate::iface! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                { $($unexpanded)* }
                $($expanded)*
                fn $name($($argname: $argtype),*) -> ();
            }
        }
    };


    // match a function with return type
    (trait $traitname:ident {
        const PROTOCOL_ID: i64 = $proto:tt;
        {
            fn $name:ident($($argname:ident: $argtype:ty),*) -> $ret:ty;
            $($unexpanded:tt)*
        }
        $($expanded:tt)*
    }) => {
        $crate::iface! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                { $($unexpanded)* }
                $($expanded)*
                fn $name($($argname: $argtype),*) -> $ret;
            }
        }
    };

    // match arm for completely expanded trait definition
    (trait $traitname:ident {
        const PROTOCOL_ID: i64 = $proto:tt;
        { }
        $(
            fn $name:ident($($argname:ident: $argtype:ty),*) -> $ret:ty;
        )*
    }) => {
        $crate::iface_enumerate! {
            trait $traitname {
                const PROTOCOL_ID: i64 = $proto;
                type OpType = i32; // ToDo: should be flexible
                $(
                    fn $name(&mut self, $($argname: $argtype),*) -> $ret;
                )*
            }
        }
    };

    ($($unexpanded:tt)*) => { compile_error!("Invalid interface definition, please consult the documentation"); };
}

/*
// for debugging purposes
use crate::cap::{Cap, Untyped};
iface_enumerate! {
    trait EchoServer {
        const PROTOCOL_ID: i64 = 0xdeadbeef;
        type OpType = i32;
        fn do_something(&mut self, i: i32) -> u8;
        fn do_something_else(&mut self, ds: Cap<Untyped>, u: bool) -> ();
        fn signal(&mut self) -> ();
    }
}
*/
