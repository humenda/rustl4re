use _core::marker::PhantomData;
use l4_sys::{self,
    l4_cap_idx_t as CapIdx,
    l4_timeout_t,
    l4_utcb_t
};

use super::{
    MsgTag,
    types::{Bufferless, BufferAccess, BufferManager, Callable,
        CapProvider, Demand, Dispatch},
};
use super::super::{
    cap::Interface,
    error::{Error, Result},
    utcb::UtcbMr,
};
use libc::c_void;

/// Stack-based buffer for server objects
///
/// This buffer can be used to work with data on the stack without the need to
/// allocate on the heap. Intermediate buffers are often used in C/C++ server
/// functions to copy data from the virtual registers to the stack frame (a
/// function-local array). This is impossible in Rust, since data is not copyied
/// manually and the array is invalidated on function return.  Including the
/// buffer in the server object moves the same idea to a different location on
/// the stack, but with the same effect and with Rust's memory management
/// working.
pub trait StackBuf {
    /// Retrieve a mutable slice of the buffer
    #[inline]
    fn get_buffer(&mut self) -> &mut [u8];

    /// Return the length of the cache in bytes
    #[inline]
    fn get_buffer_size(&self) -> usize;
}

/// Fill a buffer with data and get a typed reference to it
///
/// To allow the usage of different data types within the same buffer, this
/// trait abstracts from the concrete type and allows to copy the input data
/// into the "byte-buffer" and returns a correctly typed reference to it.
/// This is used for interface implementations returning objects with unknown
/// compile-time size such as `&str` or `&[T]`. See the `buffer` parameter of
/// the [`l4_server` macro](../../../l4_derive/macro.l4_server.html).
pub trait TypedBuffer<T>
        where T: ?Sized + core::borrow::Borrow<T> + core::borrow::BorrowMut<T> {
    /// Fill the cache with a copy of the given input an return a mutable
    /// reference to it
    #[inline]
    fn copy_in(&mut self, i: &T) -> Result<&mut T>;
}

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
    fn ipc_error<C>(&mut self, _: &mut Loop<Self, C>, tag: &MsgTag)
                -> LoopAction
            where Self: ::_core::marker::Sized, C: CapProvider {
        match (*tag).clone().result() {
            Ok(_) => unreachable!(),
            Err(e) => LoopAction::ReplyAndWait(MsgTag::new(match e {
                    Error::Generic(e) => e as i64,
                    Error::Tcr(e) => e as i64,
                    Error::UnknownErr(e) => e as i64,
                    _ => panic!("{:?}", e),
                } * -1, 0, 0, 0))
                // ^ by convention, error codes are negative
        }
    }

    // ToDo: docs + rethink Dispatch trait and LoopHook interface to take **any** error interace
    fn application_error<C>(&mut self, _: &mut Loop<Self, C>, e: Error)
                -> LoopAction
            where Self: ::_core::marker::Sized, C: CapProvider {
        // ^ by convention, error types are returned as negative integer
        LoopAction::ReplyAndWait(MsgTag::new(e.into_ipc_err(), 0, 0, 0))
    }

    /// set default timeout for replies
    #[inline]
    fn reply_timeout() -> l4_timeout_t {
        l4_sys::consts::L4_IPC_SEND_TIMEOUT_0
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
pub fn server_impl_callback<T>(ptr: *mut c_void, tag: MsgTag,
                                  mr: &mut UtcbMr,
                                  bufs: &mut BufferAccess)
        -> Result<MsgTag>
        where T: Callable + Dispatch {
    unsafe {
        let ptr = ptr as *mut T;
        (*ptr).dispatch(tag, mr, bufs)
    }
}

// first c_void is the server struct, the second c_void is the ArgAccess instance
pub type Callback = fn(*mut libc::c_void, MsgTag, &mut UtcbMr,
                       &mut BufferAccess) -> Result<MsgTag>;

pub struct Loop<'a, Hooks: LoopHook, BrMgr> {
    /// thread that this server runs in
    thread: CapIdx,
    /// UTCB reference
    pub utcb: *mut l4_utcb_t,
    /// Loop hooks
    hooks: Option<Hooks>, // prevents double mut borrow
    // buffer manager (receive windows for kernel items)
    buf_mgr: BrMgr,
    /// lifetime for registered servers from outside
    outlive_me: PhantomData<&'a Self>
}

// allow unwrapping the option from the struct above, using the value and
// injecting it back
macro_rules! borrow {
    ($sel:ident.hooks.$func:ident(&mut self, $($arg:expr),*)) => {{ // new scope
        let mut hooks = $sel.hooks.take().unwrap(); // option is never none
        let res = hooks.$func($sel, $($arg),*);
        $sel.hooks = Some(hooks);
        res
    }}
}

impl<'a, T: LoopHook, C: CapProvider> Loop<'a, T, C> {
    /// Create new server loop at given thread with specified hooks
    fn new(thread: CapIdx, u: *mut l4_utcb_t, h: T, b: C) -> Loop<'a, T, C> {
        Loop {
            thread: thread,
            utcb: u,
            hooks: Some(h),
            buf_mgr: b,
            outlive_me: PhantomData,
        }
    }

    /// Register anything, callee must make sure that passed thing is NOT moved
    pub fn register<Imp>(&mut self, inst: &'a mut Imp) -> Result<()>
            where Imp: Callable + Dispatch + Demand + Interface {
        self.buf_mgr.alloc_capslots(<Imp as Demand>::CAP_DEMAND)?;
        unsafe { // bind IPC gate with label
            let _ = MsgTag::from(l4_sys::l4_rcv_ep_bind_thread(inst.raw(),
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
    #[inline]
    fn reply_and_wait(&mut self, replytag: MsgTag, label: &mut u64,
            client_tag: &mut MsgTag) {
        self.buf_mgr.setup_wait(self.utcb);
        unsafe {
            *client_tag = MsgTag::from(l4_sys::l4_ipc_reply_and_wait(self.utcb,
                     replytag.raw(), label as _, l4_sys::timeout_never()));
            // avoid broken IPC to kill the loop
            if *label == 0 && client_tag.has_error() {
                *client_tag = MsgTag::from(l4_sys::l4_ipc_wait(self.utcb,
                        label, l4_sys::timeout_never()));
            }
        }
    }

    /// Start the server loop
    ///
    /// # Panics
    ///
    /// The function will panic if no server implementations were registered.
    pub fn start(&mut self) {
        let mut label: u64 = 0;
        // called once here for initial wait (see reply_and_wait)
        self.buf_mgr.setup_wait(self.utcb);
        // initial (open) wait before loop
        let mut tag = unsafe { MsgTag::from(l4_sys::l4_ipc_wait(self.utcb,
                &mut label, l4_sys::timeout_never()))
        };
        loop {
            let replytag = match tag.has_error() {
                // call user-registered error handler
                true => match borrow!(self.hooks.ipc_error(&mut self, &tag)) {
                    LoopAction::Break => break,
                    LoopAction::ReplyAndWait(t) => t,
                },

                // use label to dispatch to registered server implementation
                false => match self.dispatch(tag.clone(), label) {
                    LoopAction::Break => break,
                    LoopAction::ReplyAndWait(tag) => tag,
                }
            };
            self.reply_and_wait(replytag, &mut label, &mut tag);
        }
    }

    fn dispatch(&mut self, tag: MsgTag, ipc_label: u64) -> LoopAction {
        // create argument reader / writer instance with access to the message registers and the
        // allocated (cap) buffers
        let mut mr = unsafe { UtcbMr::from_utcb(self.utcb) };
        // we rely on the user implementation to be auto-derived by the
        // framework and hence that it reported the correct demand so that this
        // pointer type accesses valid memory
        let mut bufs = unsafe { self.buf_mgr.access_buffers() };
        let result = unsafe {
            let handler = ipc_label as *mut c_void;
            let callable = *(ipc_label as *mut Callback);
            callable(handler, tag, &mut mr, &mut bufs)
        };
        // dispatch received result to user-registered handler
        match result {
            Err(e) => borrow!(self.hooks.application_error(&mut self, e)),
            Ok(tag) => LoopAction::ReplyAndWait(tag),
        }
    }
}

pub struct DefaultHooks;

impl LoopHook for DefaultHooks { }

pub struct LoopBuilder<H, C> {
    thread: CapIdx,
    utcb: *mut l4_utcb_t,
    buf_mgr: C,
    hooks: H
}

impl LoopBuilder<DefaultHooks, BufferManager> {
    pub unsafe fn new_at(thcap: CapIdx, u: *mut l4_utcb_t) -> Self {
        LoopBuilder {
            thread: thcap,
            utcb: u,
            buf_mgr: BufferManager::new(),
            hooks: DefaultHooks { },
        }
    }
}

impl<H: LoopHook, C: CapProvider> LoopBuilder<H, C> {
    /// Register a different loop hooks
    pub fn custom_hooks<I: LoopHook>(self, h: I) -> LoopBuilder<I, C> {
        LoopBuilder {
            thread: self.thread,
            utcb: self.utcb,
            buf_mgr: self.buf_mgr,
            hooks: h,
        }
    }

    /// A server loop unable to allocate capability slots. Useful for very simple IPC services with
    /// performance constraints.
    pub fn no_buffers(self) -> LoopBuilder<H, Bufferless> {
        LoopBuilder {
            thread: self.thread, 
            utcb: self.utcb,
            buf_mgr: Bufferless { },
            hooks: self.hooks,
        }
    }

    /// Costruct a new server loop builder without buffer allocation
    ///
    /// This constructs a new server loop builder which does not offer buffer allocation, e.g. to
    /// receive capabilities.
    ///
    /// # Safety
    ///
    /// As long as the UTCB pointer is valid, this function is safe.
    pub unsafe fn new_at_bufferless(thread: CapIdx, u: *mut l4_utcb_t)
            -> LoopBuilder<DefaultHooks, Bufferless> {
        LoopBuilder {
            thread: thread,
            utcb: u,
            buf_mgr: Bufferless { },
            hooks: DefaultHooks { },
        }
    }

    pub fn build<'a>(self) -> Loop<'a, H, C> {
        Loop::new(self.thread, self.utcb, self.hooks, self.buf_mgr)
    }
}
