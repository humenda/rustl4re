//! Simple client-server example
//!
//! The client sends a number, the server squares it and returns it. That's it.

extern crate l4_sys as l4;
extern crate libc;

use std::{thread, time};

use l4::{l4_cap_idx_t, l4_umword_t, l4_utcb, msgtag, timeout_never};

// ToDo: use from libc

fn client(server: l4_cap_idx_t) {
    let mut counter = 1;
    loop {
        // dump value into Userspace Thread Control Block (UTCB)
        unsafe {
            (*l4::l4_utcb_mr()).mr.as_mut()[0] = counter;
        }
        println!("client: value written to register");
        // create message tag; specifies kind of interaction between sender and receiver: protocol,
        // number of machine words and flex pages to send and other flags (ignore flex pages in
        // this example);
        //
        // no protocol, 1 machine word, 0 flex pages, no flags
        let send_tag = msgtag(0, 1, 0, 0);
        unsafe {
            // IPC call to capability of server
            let rcv_tag = l4::l4_ipc_call(server, l4_utcb(), send_tag,
                    timeout_never());
            println!("send worked");
            // check for IPC error, if yes, print out the IPC error code, if not, print the received
            // result.
            match l4::l4_ipc_error(rcv_tag, l4_utcb()) {
                0 => // success
                    println!("Received: {}\n", (*l4::l4_utcb_mr()).mr.as_ref()[0]),
                ipc_error => println!("client: IPC error: {}\n",  ipc_error),
            };
        }
        // even servers need to relax
        thread::sleep(time::Duration::from_millis(1000));
        counter += 1;
    }
}

/// Server part.
fn main() {
    // get thread capability using the pthread interface
    let server: l4_cap_idx_t = unsafe {
        l4::pthread_l4_cap(libc::pthread_self()) };
    let _client = thread::spawn(move || client(server));
    // label is used to identify senders (not relevant in our example)
    println!("beef");
    let mut label: l4_umword_t = unsafe { ::std::mem::uninitialized() };
    println!("aaaf");
    // square server
    // Wait for requests from any thread. No timeout, i.e. wait forever.
    println!("server: waiting for incoming connections");
    // wait infinitely for messages, see client for more
    let mut tag = unsafe { l4::l4_ipc_wait(l4::l4_utcb(), &mut label,
            timeout_never()) };
    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        unsafe {
            let ipc_error = l4::l4_ipc_error(tag, l4::l4_utcb());
            if ipc_error != 0 {
                println!("server: IPC error: {}\n", ipc_error);
                tag = l4::l4_ipc_wait(l4::l4_utcb(), &mut label, timeout_never());
                continue;
            }
        }

        // the IPC was ok, now take the value out of message register 0 of the
        // UTCB and store the square of it back to it.
        unsafe {
            let val = (*l4::l4_utcb_mr()).mr.as_ref()[0];
            (*l4::l4_utcb_mr()).mr.as_mut()[0] = val * val;
            println!("new value: {}", (*l4::l4_utcb_mr()).mr.as_ref()[0]);

            // send reply and wait again for new messages.
            println!("send reply, wait for incoming connections");
            // msgtag: transfer 1 word from message registers (also see client)
            tag = l4::l4_ipc_reply_and_wait(l4::l4_utcb(), msgtag(0, 1, 0, 0),
                                  &mut label, timeout_never());
        }
    }
}
