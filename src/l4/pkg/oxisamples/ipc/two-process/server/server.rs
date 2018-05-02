#![feature(libc)]
// ToDo: ^ -> replace with version from crates.io

extern crate ipc_sys as ipc;
extern crate cap_sys as cap;
extern crate libc;


use ipc::{l4_msgtag, l4_umword_t};

// ToDo: use from libc

#[no_mangle]
pub extern "C" fn main() {
    let mut label: l4_umword_t = unsafe { ::std::mem::uninitialized() };
    // square server
    // Wait for requests from any thread. No timeout, i.e. wait forever.
    let mut tag = unsafe { ipc::l4_ipc_wait(ipc::l4_utcb(), &mut label,
            ipc::l4_timeout_t { raw: 0 }) };
    println!("waiting for incoming connections");
    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        unsafe {
            let ipc_error = ipc::l4_ipc_error(tag, ipc::l4_utcb());
            if ipc_error != 0 {
                println!("server: IPC error: {}\n", ipc_error);
                tag = ipc::l4_ipc_wait(ipc::l4_utcb(), &mut label, 
                        ipc::l4_timeout_t { raw: 0 });
                continue;
            }
        }

        // the IPC was ok, now take the value out of message register 0 of the
        // UTCB and store the square of it back to it.
        unsafe {
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
}

/*
#[no_mangle]
pub extern "C" fn main() {
    println!("âye up");
}
*/
