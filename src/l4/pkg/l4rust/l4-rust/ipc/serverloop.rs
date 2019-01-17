use _core::{
    marker::PhantomData,
    mem
};
use l4_sys::{self,
    l4_buf_regs_t,
    l4_cap_consts_t::{L4_INVALID_CAP_BIT, L4_CAP_MASK},
    l4_cap_idx_t as CapIdx, l4_timeout_t,
    l4_msg_item_consts_t::L4_RCV_ITEM_SINGLE_CAP,
    l4_utcb_t, l4_utcb_br_u};

use super::{
    MsgTag, types::{Callable, Demand, Dispatch},
};
use super::super::{
    cap::{Cap, Interface, IfaceInit},
    error::{Error, GenericErr, Result},
    types::UMword,
};
use libc::c_void;

/// Action instructions from hooks to the server loop.
pub enum LoopAction {
    /// Reply to the client and change to open wait afterwards.
    ///
    /// This skips the execution of the whole loop and replies with the given message tag to the
    /// client, e.g. to signal an error. If items are to be transfered, it is the handlers
    /// responsibility to prepare the message registers.
    ReplyAndWait(MsgTag),
    /// Unconditionally break the server loop.
    ///
    /// This will immediately exit the server loop without any client reply. Use with great
    /// caution.
    Break,
}

/// Register hooks in the server loop
///
/// This trait allows the specialisation of the server loop by overriding the desired functions of
/// a default hook implementation with custom values. Due to the default implementations of the
/// functions, an implementor can pick which hooks to override.
pub trait LoopHook {
    /// IPC Error handler function
    ///
    /// The implementor may implement an IPC error handling strategie which has
    /// access to the complete thread and server loop state.
    /// The returned action directly influences the execution of the loop, see the documentation of
    /// [LoopAction](enum.LoopAction.html).
    fn ipc_error(&mut self, _: &mut Loop<Self>, tag: MsgTag) -> LoopAction
            where Self: ::_core::marker::Sized {
        match tag.result() {
            Ok(_) => unreachable!(),
            Err(e) => LoopAction::ReplyAndWait(MsgTag::new(match e {
                    Error::Generic(e) => e as i64,
                    Error::Tcr(e) => e as i64,
                    Error::UnknownErr(e) => e as i64,
                    Error::InvalidCap | Error::InvalidArg(_, _)
                            | Error::Protocol(_) | Error::InvalidState(_) =>
                        panic!("unsupported error type received"),
                }, 0, 0, 0))
        }
    }

    // ToDo: docs + rethink Dispatch trait and LoopHook interface to take **any** error interace
    fn application_error(&mut self, _: &mut Loop<Self>, e: Error) -> LoopAction
            where Self: ::_core::marker::Sized {
        LoopAction::ReplyAndWait(MsgTag::new(match e {
                    Error::Generic(e) => e as i64,
                    Error::Tcr(e) => e as i64,
                    Error::UnknownErr(e) => e as i64,
                    Error::InvalidCap | Error::InvalidArg(_, _)
                            | Error::Protocol(_) | Error::InvalidState(_) =>
                        panic!("unsupported error type received"),
                }, 0, 0, 0))
    }


    /// set default timeout for replies
    fn reply_timeout() -> l4_timeout_t {
        l4_sys::consts::L4_IPC_SEND_TIMEOUT_0
    }

    /// Configure the buffer registers for receive operations before the reply
    /// operation. The default implementation automatically allocates capability
    /// slots according to the maximum required demand.
    fn setup_wait(_: *mut l4_utcb_t) { }
}

/// Receive window management for IPC server loop(s)
///
/// A buffer manager controls the capability slot allocation and the buffer register setup to
/// receive and dispatch kernel **items** such as capability flexpages. How and whether
/// capabilities (etc.) are allocated depends on the policy of each implementor.
pub trait BufDemand {
    /// Allocate buffers according to given demand
    ///
    /// This function will allocate as many buffers as required for the given
    /// capability demand. If buffers were allocated before, these are left
    /// unchanged, only the additional buffers are allocated. If the  given
    /// demand is smaller than the actual number of allocated buffers, no action
    /// is performed.  
    /// A capability beyond the internal buffer size will resultin an
    /// `Error::InvalidArg` and capability allocation failures in an
    /// `Error::NoMem`.
    /// **Note:** memory or I/O flexpages are not supported.
    fn alloc_capslots(&mut self, demand: u32) -> Result<()>;

    /// Retrieve capability at allocated buffer slot/index
    ///
    /// The argument must be within `0 <= index <= self.caps_used`.
    fn rcv_cap<T>(&self, index: usize) -> Result<Cap<T>>
                where T: Interface + IfaceInit;
    /// Set the receive flags for the buffers.
    ///
    /// This must be called **before** any buffer has been allocated and fails otherwise.
    fn set_rcv_cap_flags(&mut self, flags: UMword) -> Result<()>;

    /// Return the maximum number of slots this manager could allocate
    #[inline]
    fn max_slots(&self) -> u32;

    /// Return the amount of allocated buffer slots.
    #[inline]
    fn caps_used(&self) -> u32;

    /// Set up the UTCB for receiving items
    ///
    /// This function instructs where and how to map flexpages on arrival. It is
    /// usually called by the server loop.
    fn setup_wait(&mut self, _: *mut l4_utcb_t);
}

/// A buffer manager not able to receive any capability.
///
/// While this limits the server implementations, it might be a handy
/// optimisation, saving any buffer setup. Since this is a zero-sized struct,
/// it'll also shrink the memory footprint of the service.
pub struct Bufferless;

impl BufDemand for Bufferless {
    fn alloc_capslots(&mut self, demand: u32) -> Result<()> {
        match demand == 0 {
            true => Ok(()),
            false => Err(Error::InvalidArg("buffer allocation not permitted", None)),
        }
    }

    fn rcv_cap<T>(&self, _: usize) -> Result<Cap<T>>
                where T: Interface + IfaceInit {
        Err(Error::InvalidArg("No buffers allocated", None))
    }

    fn set_rcv_cap_flags(&mut self, _: UMword) -> Result<()> { Ok(()) }

    #[inline]
    fn max_slots(&self) -> u32 { 0 }
    #[inline]
    fn caps_used(&self) -> u32 { 0 }
    fn setup_wait(&mut self, _: *mut l4_utcb_t) { }
}

pub struct BufferManager {
    /// number of allocated capabilities
    caps: u32,
    cap_flags: u64,
    // ToDo: reimplement this with the (ATM unstable) const generics
    br: [u64; l4_sys::consts::UtcbConsts::L4_UTCB_GENERIC_BUFFERS_SIZE as usize],
}

impl BufDemand for BufferManager {
    fn alloc_capslots(&mut self, demand: u32) -> Result<()> {
        // take two extra buffers for a possible timeout and a zero terminator
        if demand + 3 >= self.br.len() as u32 {
            return Err(Error::InvalidArg("Capability slot demand too large",
                                         Some(demand as isize)));
        }
        while demand > self.caps {
            let cap = unsafe { l4_sys::l4re_util_cap_alloc() };
            if (cap & L4_INVALID_CAP_BIT as u64) == 0 {
                return Err(Error::Generic(GenericErr::NoMem));
            }

            // safe this cap as a "receive item" in the format that the kernel
            // understands
            self.br[self.caps as usize] = cap | L4_RCV_ITEM_SINGLE_CAP as u64
                    | self.cap_flags;
            self.caps += 1;
        }
        self.br[self.caps as usize] = 0; // terminate receive list
        Ok(())
    }

    fn rcv_cap<T>(&self, index: usize) -> Result<Cap<T>>
            where T: Interface + IfaceInit {
        match index >= self.caps as usize {
            true  => Err(Error::InvalidArg("Index not allocated", Some(index as isize))),
            false => Ok(Cap::<T>::new(self.br[index] & L4_CAP_MASK as u64)),
        }
    }

    fn set_rcv_cap_flags(&mut self, flags: UMword) -> Result<()> {
        if self.cap_flags == 0 {
            self.cap_flags = flags as u64;
            return Ok(());
        }
        Err(Error::InvalidState("Unable to set buffer flags when capabilities \
                                were already allocated"))
    }

    #[inline]
    fn max_slots(&self) -> u32 {
        self.br.len() as u32
    }

    #[inline]
    fn caps_used(&self) -> u32 {
        self.caps
    }

    fn setup_wait(&mut self, u: *mut l4_utcb_t) {
        unsafe {
            let br: *mut l4_buf_regs_t = l4_utcb_br_u(u);
            (*br).bdr = 0;
            for index in 0usize..(self.caps as usize - 1) {
                (*br).br[index] = self.br[index];
            }
        }
    }
}
/// Server implementation callback from server loop
///
/// Whenever a server object is registered with the server loop, the label associated with the
/// incoming messages is chosen to be the memory address to the server implementation object. Each
/// server implementation contains a function pointer able to call the dispatch method of the
/// server object. The `Callable` trait enforces this struct layout. The
/// function pointer is usually chosen to be a specialised version of the
/// generic function below. The fact that the label (AKA object) pointer points
/// to the server implementation and that the implementation itself contains the
/// function pointer using a void pointer as the self argument bypasses the need
/// for a trait object.  
/// For users of this function, it is usually enough to assign
/// `server_impl_callback::<Self>` to the first struct member.
/// generic function is
/// usually used as the function pointed to 
pub fn server_impl_callback<T: Callable + Dispatch>(ptr: *mut c_void,
        tag: MsgTag, utcb: *mut l4_utcb_t) -> Result<MsgTag> {
    unsafe {
        let ptr = ptr as *mut T;
        (*ptr).dispatch(tag, utcb)
    }
}

pub type Callback = fn(*mut libc::c_void, MsgTag, *mut l4_utcb_t) -> Result<MsgTag>;

pub struct Loop<'a, Hooks: LoopHook> {
    /// thread that this server runs in
    thread: CapIdx,
    /// UTCB reference
    pub utcb: *mut l4_utcb_t,
    /// Loop hooks
    hooks: Option<Hooks>, // prevents double mut borrow
    /// lifetime for registered servers from outside
    outlive_me: PhantomData<&'a Loop<'a, Hooks>>
}

macro_rules! borrow {
    ($sel:ident.hooks.$func:ident(&mut self, $($arg:ident),*)) => {{ // new scope
        let mut hooks = $sel.hooks.take().unwrap(); // option is never none
        let res = hooks.$func($sel, $($arg),*);
        $sel.hooks = Some(hooks);
        res
    }}
}

impl<'a> Loop<'a, DefaultHooks> {
    /// Create new server loop at given thread
    pub fn new_at(thread: CapIdx, u: *mut l4_utcb_t) -> Loop<'a, DefaultHooks> {
        Loop {
            thread: thread,
            utcb: u,
            hooks: Some(DefaultHooks { }),
            outlive_me: PhantomData,
        }
    }
}

impl<'a, T: LoopHook> Loop<'a, T> {
    /// Create new server loop at given thread with specified hooks
    pub fn new_custom_at(thread: CapIdx, u: *mut l4_utcb_t, h: T) -> Loop<'a, T> {
        Loop {
            thread: thread,
            utcb: u,
            //servers: Vec::new(),
            hooks: Some(h),
            outlive_me: PhantomData,
        }
    }

    /// Register anything, callee must make sure that passed thing is NOT moved
    pub fn register<Imp: Callable + Dispatch + Demand>(&mut self, gate: CapIdx,
            inst: &'a mut Imp) -> Result<()> {
        unsafe {
            let _ = MsgTag::from(l4_sys::l4_rcv_ep_bind_thread(gate,
                   self.thread, inst as *mut _ as *mut c_void as _))
                .result()?;
        }
        Ok(())
    }

    /// Reply and wait operation
    ///
    /// Replying to the client and waiting for the next request is a joint operation on L4Re. The
    /// C++ version also supports the less performant split of a reply and a wait system call
    /// afterwards, which is omitted here.
    fn reply_and_wait(&mut self, replytag: MsgTag) -> (u64, MsgTag) {
        // ToDo: missing: setup_wait function
        unsafe {
            let mut label = mem::uninitialized();
            let tag = MsgTag::from(l4_sys::l4_ipc_reply_and_wait(self.utcb,
                     replytag.raw(), &mut label, l4_sys::timeout_never()));
            (label, tag)
        }
    }

    /// Start the server loop
    ///
    /// # Panics
    ///
    /// The function will panic if no server implementations were registered.
    pub fn start(&mut self) {
        let mut label: u64 = 0;
        // initial (open) wait before loop
        let mut tag = unsafe { MsgTag::from(l4_sys::l4_ipc_wait(self.utcb,
                &mut label, l4_sys::timeout_never()))
        };
        loop {
            let replytag = match tag.has_error() {
                // call user-registered error handler
                true => match borrow!(self.hooks.ipc_error(&mut self, tag)) {
                    LoopAction::Break => break,
                    LoopAction::ReplyAndWait(t) => t,
                },

                // use label to dispatch to registered server implementation
                false => {
                    match self.dispatch(tag, label) {
                    LoopAction::Break => break,
                    LoopAction::ReplyAndWait(tag) => tag,
                }}
            };
            let (tmp1, tmp2) = self.reply_and_wait(replytag);
            label = tmp1; // prevent shadowing
            tag = tmp2;
        }
    }

    fn dispatch(&mut self, tag: MsgTag, ipc_label: u64) -> LoopAction {
        let result = unsafe {
            let handler = ipc_label as *mut c_void;
            let callable = *(ipc_label as *mut Callback);
            callable(handler, tag, self.utcb)
        };
        // dispatch received data to user-registered handler
        match result {
            Err(e) => borrow!(self.hooks.application_error(&mut self, e)),
            Ok(tag) => LoopAction::ReplyAndWait(tag),
        }
    }
}

pub struct DefaultHooks;

impl LoopHook for DefaultHooks { }
