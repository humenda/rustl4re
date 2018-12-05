//! A simple square server (squaring a number for a client). This server listens on an IPC gate for
//! incoming connections of arbitrary clients.
// This doesn't work with stable Rust, use "extern crate libc" here:
#![feature(libc)]
extern crate l4_sys as l4;
extern crate l4re;
extern crate libc;

use l4::{l4_ipc_error, l4_msgtag, l4_utcb};

pub fn main() {
    unsafe { // avoid deep nesting of unsafe blocks, just make the whole program unsafe
        unsafe_main();
    }
}

pub unsafe fn unsafe_main() {
    // IPC gates can receive from multiple senders, hence messages need an idification label for
    // clients. Here's one:
    let gatelabel = 0b11111100 as u64;
    // place to put label in when a message is received
    let mut label = std::mem::uninitialized();


    // get IPC gate capability from Lua script (see ../*.cfg)
    let gate = l4re::sys::l4re_env_get_cap("channel").unwrap();
    // check whether we got something
    if l4::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }

    // bind IPC gate to main thread with custom label
    match l4_ipc_error(l4::l4_rcv_ep_bind_thread(gate,
            (*l4re::sys::l4re_env()).main_thread, gatelabel), l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");

    println!("waiting for incoming connections");
    // Wait for requests from any thread. No timeout, i.e. wait forever. Note that the label of the
    // sender will be placed in `label`.
    let mut tag = l4::l4_ipc_wait(l4::l4_utcb(), &mut label,
            l4::l4_timeout_t { raw: 0 });
    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        let ipc_error = l4::l4_ipc_error(tag, l4::l4_utcb());
        if ipc_error != 0 {
            println!("server: IPC error: {}\n", ipc_error);
            tag = l4::l4_ipc_wait(l4::l4_utcb(), &mut label, 
                    l4::l4_timeout_t { raw: 0 });
            continue;
        }

        // IPC was ok, now get value from message register 0 of UTCB and store the square of it
        // back to it
        let val = (*l4::l4_utcb_mr()).mr[0];
        (*l4::l4_utcb_mr()).mr[0] = val * val;
        println!("new value: {}", (*l4::l4_utcb_mr()).mr[0]);

        // send reply and wait again for new messages; the message tag specifies to send one
        // machine word with no protocol, no flex pages and no flags
        tag = l4::l4_ipc_reply_and_wait(l4::l4_utcb(), l4_msgtag(0, 1, 0, 0),
                              &mut label, l4::l4_timeout_t { raw: 0 });
    }
}

