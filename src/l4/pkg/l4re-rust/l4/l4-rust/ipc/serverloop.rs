use alloc::{boxed::Box, vec::Vec};
use l4_sys::{self, l4_cap_idx_t as CapIdx, l4_utcb_t};

use super::{MsgTag, types::Dispatch};
use super::super::error::Result;

pub struct Loop {
    /// thread that this server runs in
    thread: CapIdx,
    /// UTCB reference
    utcb: *mut l4_utcb_t,
    servers: Vec<Box<Dispatch>>,
}

impl Loop {
    pub fn new_at(thread: CapIdx) -> Loop {
        Loop {
            thread: thread,
            utcb: unsafe { l4_sys::l4_utcb() },
            servers: Vec::new(),
        }
    }

    pub fn register<T>(&mut self, gate: CapIdx, inst: T) -> Result<()>
            where T: Dispatch + 'static {
        unsafe {
            // pass in vector index as label (ToDo: can I directly use the
            // address to the object in compliance with  the borrow checker?
            let _ = MsgTag::from(l4_sys::l4_rcv_ep_bind_thread(gate,
                   self.thread, self.servers.len() as u64))
                .result()?;
        }
        self.servers.push(Box::new(inst));
        Ok(())
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
            // ToDo: allow error handling hooks
            let _ = tag.result().unwrap();
            // The server_id was registered with the kernel and hence we can trust it to always point
            // into the vector.
            let replytag = { // own ns for handler to prevent double mut borrow
                let mut handler = unsafe {
                    self.servers.get_unchecked_mut(server_id as usize)
                };
                handler.dispatch().unwrap() // ToDo: error handling
            };
            let (tmp1, tmp2) = self.reply_and_wait(replytag);
            server_id = tmp1;
            tag = tmp2;
        }
    }
}
// ToDo:
// -    function to allocate required receive buffers
// -    setup function for buffers:
//    void setup_wait(l4_utcb_t *utcb, L4::Ipc_svr::Reply_mode) {
//        l4_buf_regs_t *br = l4_utcb_br_u(utcb);
//        br->bdr = 0;
//        br->br[0] = 0;
//    }
// - ToDo: concept of Ignore_errors, Default_timeout, Compound_reply, and Br_manager_no_buffers
