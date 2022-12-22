//! A simple square server (squaring a number for a client). This server listens on an IPC gate for
//! incoming connections of arbitrary clients.

#![no_std]

use l4::{
    println,
    sys::{l4_ipc_error, l4_msgtag},
};

#[cfg(feature = "std")]
pub fn main() {
    main_body();
}

#[cfg(not(feature = "std"))]
pub extern "C" fn main(_a: u32, _ptr: *const u8) {
    main_body();
}

pub fn main_body() {
    // IPC gates can receive from multiple senders, hence messages need an idification label for
    // clients. Here's one:
    let gatelabel = 0b11111100 as u64;
    // place to put label in when a message is received
    let mut label = 0;

    // get IPC gate capability from Lua script (see ../*.cfg)
    let gate = 0; // ToDounsafe { l4::sys::l4re_env_get_cap("channel").unwrap() };
                  // check whether we got something
    if l4::sys::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }

    // bind IPC gate to main thread with custom label
    match unsafe {
        l4_ipc_error(
            l4::sys::l4_rcv_ep_bind_thread(gate, (*l4re::sys::l4re_env()).main_thread, gatelabel),
            l4::sys::l4_utcb(),
        )
    } {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n),
    };
    println!("IPC gate received and bound.");

    println!("waiting for incoming connections");
    // Wait for requests from any thread. No timeout, i.e. wait forever. Note that the label of the
    // sender will be placed in `label`.
    let mut tag = unsafe {
        l4::sys::l4_ipc_wait(
            l4::sys::l4_utcb(),
            &mut label,
            l4::sys::l4_timeout_t { raw: 0 },
        )
    };
    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        let ipc_error = unsafe { l4::sys::l4_ipc_error(tag, l4::sys::l4_utcb()) };
        if ipc_error != 0 {
            println!("server: IPC error: {}\n", ipc_error);
            tag = unsafe {
                l4::sys::l4_ipc_wait(
                    l4::sys::l4_utcb(),
                    &mut label,
                    l4::sys::l4_timeout_t { raw: 0 },
                )
            };
            continue;
        }

        // IPC was ok, now get value from message register 0 of UTCB and store the square of it
        // back to it
        let val = unsafe { (*l4::sys::l4_utcb_mr()).mr[0] };
        unsafe { (*l4::sys::l4_utcb_mr()).mr[0] = val * val };
        println!("new value: {}", (*l4::sys::l4_utcb_mr()).mr[0]);

        // send reply and wait again for new messages; the message tag specifies to send one
        // machine word with no protocol, no flex pages and no flags
        tag = unsafe {
            l4::sys::l4_ipc_reply_and_wait(
                l4::sys::l4_utcb(),
                l4_msgtag(0, 1, 0, 0),
                &mut label,
                l4::sys::l4_timeout_t { raw: 0 },
            )
        };
    }
}
