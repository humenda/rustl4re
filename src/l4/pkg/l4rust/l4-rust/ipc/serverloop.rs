use alloc::{boxed::Box, vec::Vec};
use l4_sys::{self, l4_cap_idx_t as CapIdx, l4_timeout_t, l4_utcb_t};

use super::{
    MsgTag, types::Dispatch,
    super::error::{Error, Result},
};

/// Action instructions from hooks to the server loop.
pub enum LoopAction {
    /// Reply to the client and change to open wait afterwards.
    ///
    /// This skips the execution of the whole loop and replies with the given message tag to the
    /// client, e.g. to signal an error. If items are to be transfered, it is the handlers
    /// resposibility to prepare the message registers.
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
                            | Error::Protocol(_) =>
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
                            | Error::Protocol(_) =>
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

pub struct Loop<Hooks: LoopHook> {
    /// thread that this server runs in
    thread: CapIdx,
    /// UTCB reference
    pub utcb: *mut l4_utcb_t,
    /// List of registered server implementations
    //pub servers: Vec<Box<Dispatch>>,
    /// Loop hooks
    hooks: Option<Hooks>, // prevents double mut borrow
}

macro_rules! borrow {
    ($sel:ident.hooks.$func:ident(&mut self, $($arg:ident),*)) => {{ // new scope
        let mut hooks = $sel.hooks.take().unwrap(); // option is never none
        let res = hooks.$func($sel, $($arg),*);
        $sel.hooks = Some(hooks);
        res
    }}
}

impl Loop<DefaultHooks> {
    /// Create new server loop at given thread
    pub fn new_at(thread: CapIdx, u: *mut l4_utcb_t) -> Loop<DefaultHooks> {
        Loop {
            thread: thread,
            utcb: u,
            //servers: Vec::new(),
            hooks: Some(DefaultHooks { }),
        }
    }
}

impl<T: LoopHook> Loop<T> {
    /// Create new server loop at given thread with specified hooks
    pub fn new_custom_at(thread: CapIdx, u: *mut l4_utcb_t, h: T) -> Loop<T> {
        Loop {
            thread: thread,
            utcb: u,
            //servers: Vec::new(),
            hooks: Some(h),
        }
    }
    pub fn register<Imp>(&mut self, gate: CapIdx, inst: Imp)
            -> Result<()>
            where Imp: Dispatch + 'static {
        unimplemented!();
        /*
        unsafe {
            // pass in vector index as label (ToDo: can I directly use the
            // address to the object in compliance with  the borrow checker?
            let _ = MsgTag::from(l4_sys::l4_rcv_ep_bind_thread(gate,
                   self.thread, self.servers.len() as u64))
                .result()?;
        }
        self.servers.push(Box::new(inst));
        Ok(())
        */
    }

    /// Reply and wait operation
    ///
    /// Replying to the client and waiting for the next request is a joint operation on L4Re. The
    /// C++ version also supports the less performant split of a reply and a wait system call
    /// afterwards, which is omitted here.
    fn reply_and_wait(&mut self, replytag: MsgTag) -> (u64, MsgTag) {
        // ToDo: missing: setup_wait function
        let mut label = 0u64;
        (label, MsgTag::from(unsafe {
            l4_sys::l4_ipc_reply_and_wait(self.utcb, replytag.raw(), &mut label,
                l4_sys::timeout_never())
        }))
    }

    /// Start the server loop
    ///
    /// # Panics
    ///
    /// The function will panic if no server implementations were registered.
    pub fn start(&mut self) {
        unimplemented!();
        /*
        if self.servers.len() == 0 {
            panic!("Failed to start server loop, no server implementation(s) \
                    registered.");
        }
        let mut server_id: u64 = 0;
        // initial (open) wait before loop
        let mut tag = unsafe { MsgTag::from(l4_sys::l4_ipc_wait(self.utcb,
                &mut server_id, l4_sys::timeout_never()))
        };
        loop {
            let replytag = match tag.has_error() {
                // call user-registered error handler
                true => match borrow!(self.hooks.ipc_error(&mut self, tag)) {
                    LoopAction::Break => break,
                    LoopAction::ReplyAndWait(t) => t,
                },

                // The server_id was registered with the kernel and hence we can
                // trust it to always point into the vector.
                false => match self.dispatch(server_id) {
                    LoopAction::Break => break,
                    LoopAction::ReplyAndWait(tag) => tag,
                }
            };
            let (tmp1, tmp2) = self.reply_and_wait(replytag);
            server_id = tmp1;
            tag = tmp2;
        }
        */
    }

    fn dispatch(&mut self, server_id: u64) -> LoopAction {
        unimplemented!();
        /*
        // ToDo: what if not from registered vector, what happens if sender unknown?
        let result = {
            let mut handler = unsafe {
                self.servers.get_unchecked_mut(server_id as usize)
            };
            handler.dispatch()
        };
        // dispatch received data to user-registered handler
        match result {
            Err(e) => borrow!(self.hooks.application_error(&mut self, e)),
            Ok(tag) => LoopAction::ReplyAndWait(tag),
        }
        */
    }
}

pub struct DefaultHooks;

impl LoopHook for DefaultHooks { }
