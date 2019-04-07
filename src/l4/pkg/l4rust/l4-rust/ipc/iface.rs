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

#[cfg(bench_serialisation)]
#[macro_export]
macro_rules! derive_ipc_calls {
    ($proto:expr; $($opcode:expr =>
       fn $name:ident($($argname:ident: $type:ty),*) -> $return:ty;)*
    ) => {
        $(
            fn $name(&mut self $(, $argname: $type)*)
                        -> $crate::error::Result<$return> 
                            where Self: $crate::cap::Interface {
                                use $crate::ipc::Serialiser;
                use $crate::ipc::CapProvider;
                use core::arch::x86_64::_rdtsc as rdtsc;
                use $crate::{CLIENT_MEASUREMENTS, ClientCall};
                let mut cc = ClientCall { call_start: 0,
                        arg_serialisation_start: 0, arg_serialisation_end: 0,
                        ipc_call_start: 0, return_val_start: 0, call_end: 0 };
                { cc.call_start = unsafe { rdtsc() }; };
                // ToDo: would re-allocate a capability each time; how to control number of
                // required slots
                let mut caps = $crate::ipc::types::Bufferless { };
                let mut mr = $crate::utcb::Utcb::current().mr();
                // write opcode
                unsafe {
                    mr.write($opcode)?;
                }
                { cc.arg_serialisation_start = unsafe { rdtsc() }; };
                $crate::write_msg!(&mut mr, $($argname: $type),*)?;
                // get the protocol for the msg tag label
                unsafe { cc.arg_serialisation_end = rdtsc(); }; // MsgTag::new() is very efficient, not benchmarked
                let tag = $crate::ipc::MsgTag::new($proto, mr.words(),
                        mr.items(), 0);
                unsafe { cc.call_start = rdtsc(); }; // MsgTag::new() is very efficient, not benchmarked
                let _restag = $crate::ipc::MsgTag::from(unsafe {
                        $crate::sys::l4_ipc_call(self.raw(),
                                $crate::sys::l4_utcb(), tag.raw(),
                                $crate::sys::timeout_never())
                    }).result()?;
                unsafe { cc.return_val_start = rdtsc(); };
                mr.reset(); // read again from start of registers, `caps` still untouched
                // return () if "empty" return value, val otherwise
                let res = unsafe {
                    let mut cap_buf = caps.access_buffers();
                    <$return>::read(&mut mr, &mut cap_buf)
                };
                unsafe { cc.call_end = rdtsc(); };
                unsafe { CLIENT_MEASUREMENTS.push(cc); }
                res
            }
        )*
    }
}

/// Extract the opcode from a list of function names
///
/// The basic assumption is that all functions share the same opcode type in an IPC interface.
/// Within a macro, it is impossible to separate the first element from a type list, hence this
/// helper macro gets the head and yields the opcode type.
#[cfg(not(bench_serialisation))]
#[macro_export]
macro_rules! derive_ipc_calls {
    ($proto:expr; $($opcode:expr =>
       fn $name:ident($($argname:ident: $type:ty),*) -> $return:ty;)*
    ) => {
        $(
            fn $name(&mut self $(, $argname: $type)*)
                        -> $crate::error::Result<$return> 
                            where Self: $crate::cap::Interface {
                use $crate::ipc::Serialiser;
                use $crate::ipc::CapProvider;
                // ToDo: would re-allocate a capability each time; how to control number of
                // required slots
                let mut caps = $crate::ipc::types::Bufferless { };
                let mut mr = $crate::utcb::Utcb::current().mr();
                // write opcode
                unsafe {
                    mr.write($opcode)?;
                    $(
                        <$type>::write($argname, &mut mr)?;
                    )*
                };

                // get the protocol for the msg tag label
                let tag = $crate::ipc::MsgTag::new($proto, mr.words(),
                        mr.items(), 0);
                let _restag = $crate::ipc::MsgTag::from(unsafe {
                        $crate::sys::l4_ipc_call(self.raw(),
                                $crate::sys::l4_utcb(), tag.raw(),
                                $crate::sys::timeout_never())
                    }).result()?;
                mr.reset(); // read again from start of registers, `caps` still untouched
                // return () if "empty" return value, val otherwise
                unsafe {
                    let mut cap_buf = caps.access_buffers();
                    <$return>::read(
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
            const CAP_DEMAND: u8 = $caps:literal;
            type OpType = $op_type:tt;
            $(
                $op_code:expr => fn $name:ident(&mut self
                        $(, $argname:ident: $argtype:ty)*) -> $ret:ty;
            )*
        }
    ) => {
        pub trait $traitname: $crate::cap::Interface
                    + $crate::ipc::CapProviderAccess {
            const PROTOCOL_ID: i64 = $proto;
            const CAP_DEMAND: u8 = $caps;
            $crate::derive_ipc_calls!($proto; $($op_code =>
                    fn $name($($argname: $argtype),*) -> $ret;)*);

            // op_dispatch is auto-generated, since the dispatch "paths" are
            // known statically. To allow the automated implementation for each
            // server struct, op_dispatch is a hidden method on this trait. It
            // is hence also implemented for each client, a price that we need
            // to pay to get it auto-implemented for a server. The server then
            // implements the Dispatch trait by calling op_dispatch, which it
            // can expect to be always there.
            #[inline]
            #[doc(hidden)]
            fn op_dispatch(&mut self, tag: $crate::ipc::MsgTag,
                    mr: &mut $crate::utcb::UtcbMr,
                    bufs: &mut $crate::ipc::BufferAccess)
                    -> $crate::error::Result<$crate::ipc::MsgTag> {
                use $crate::ipc::Serialiser;
                #[cfg(bench_serialisation)]
                use $crate::SERVER_MEASUREMENTS;
                #[cfg(bench_serialisation)]
                use core::arch::x86_64::_rdtsc as rdtsc;
                #[cfg(bench_serialisation)]
                let sd = unsafe { SERVER_MEASUREMENTS.last() };
                #[cfg(bench_serialisation)]
                { sd.srv_dispatch = unsafe { rdtsc() }; };
                // uncover cheating clients
                if (tag.words() + tag.items() * $crate::utcb::WORDS_PER_ITEM)
                        > $crate::utcb::Consts::L4_UTCB_GENERIC_DATA_SIZE as usize {
                    return Err($crate::error::Error::Generic(
                            $crate::error::GenericErr::MsgTooLong))
                }
                if tag.label() != Self::PROTOCOL_ID {
                    return Err($crate::error::Error::Generic(
                            $crate::error::GenericErr::InvalidProt))
                }
                let opcode = unsafe { mr.read::<$op_type>()? };
                #[cfg(bench_serialisation)]
                { sd.opcode_dispatch = unsafe { rdtsc() }; };
                match opcode {
                $( // iterate over functions and op codes
                    $op_code => unsafe {
                        let user_ret = self.$name(
                            $(<$argtype>::read(mr, bufs)?),*
                        )?;
                        #[cfg(bench_serialisation)]
                        { sd.retval_serialisation_start = rdtsc(); };
                        mr.reset(); // reset MRs, not the bufs (not used for sending)
                        <$ret>::write(user_ret, mr)?;
                        #[cfg(bench_serialisation)]
                        { sd.result_returned = rdtsc(); };
                        // on return, label/protocol is 0 for success and a custom error code
                        // otherwise; negative means some IPC failure
                        Ok($crate::ipc::MsgTag::new(0,
                                mr.words(), mr.items(), 0))
                    },
                )* // ↓ incorrect op code
                    _ => Err($crate::error::Error::Generic(
                            $crate::error::GenericErr::InvalidProt))
                }
            }
        }
    }
}

// for debugging purposes
/*
use crate::cap::{Cap, Untyped};
iface_back! {
    trait EchoServer {
        const PROTOCOL_ID: i64 = 0xdeadbeef;
        const CAP_DEMAND: u8 = 0;
        type OpType = i32;
        0 => fn do_something(&mut self, i: i32) -> u8;
        1 => fn do_something_else(&mut self, ds: Cap<Untyped>, u: bool) -> ();
        2 => fn signal(&mut self) -> ();
        3 => fn str_without_alloc(&mut self, s: &str) -> ();
    }
}
*/
