// This doesn't work with stable Rust, use "extern crate libc" here:
#![feature(libc)]
extern crate ipc_sys as ipc;
extern crate cap_sys as cap;
extern crate libc;


use ipc::{l4_ipc_error, l4_msgtag, l4_utcb};

#[no_mangle]
pub extern "C" fn main() {
    unsafe { // avoid deep nesting of unsafe blocks
        unsafe_main();
    }
}

pub unsafe fn unsafe_main() {
    // custom identifier for messages from this label
    let gatelabel = 0b11111100 as u64;
    // receive label
    let mut label = std::mem::uninitialized();
    let gate = cap::l4re_env_get_cap("channel");
    if cap::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }

    // bind IPC gate to main thread with custom label
    match l4_ipc_error(ipc::l4_rcv_ep_bind_thread(gate,
            (*cap::l4re_env()).main_thread, gatelabel), l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");

    // square server
    // Wait for requests from any thread. No timeout, i.e. wait forever.
    println!("waiting for incoming connections");
    // wait an infinite amount of time for a message from a client
    let mut tag = ipc::l4_ipc_wait(ipc::l4_utcb(), &mut label,
            ipc::l4_timeout_t { raw: 0 });
    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        let ipc_error = ipc::l4_ipc_error(tag, ipc::l4_utcb());
        if ipc_error != 0 {
            println!("server: IPC error: {}\n", ipc_error);
            tag = ipc::l4_ipc_wait(ipc::l4_utcb(), &mut label, 
                    ipc::l4_timeout_t { raw: 0 });
            continue;
        }

        // IPC was ok, now get value from message register 0 of UTCB and store the square of it
        // back to it
        let val = (*ipc::l4_utcb_mr()).mr[0];
        (*ipc::l4_utcb_mr()).mr[0] = val * val;
        println!("new value: {}", (*ipc::l4_utcb_mr()).mr[0]);

        // send reply and wait again for new messages.
        // The '1' in msgtag indicates that 1 word transfered from first
        // register
        tag = ipc::l4_ipc_reply_and_wait(ipc::l4_utcb(), l4_msgtag(0, 1, 0, 0),
                              &mut label, ipc::l4_timeout_t { raw: 0 });
    }
}

