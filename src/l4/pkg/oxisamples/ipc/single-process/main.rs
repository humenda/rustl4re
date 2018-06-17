#![feature(libc)]
// ToDo: ^ -> replace with version from crates.io

extern crate l4_sys as l4;
extern crate libc;

use std::{thread, time};

use l4::{l4_utcb, l4_msgtag, l4_umword_t};

// ToDo: use from libc
use l4::pthread_t;

macro_rules! timeout_never {
    () => {
        l4::l4_timeout_t { raw: 0 }
    }
}

struct ThreadHandle {
    inner: pthread_t,
}

impl ThreadHandle {
    pub fn into_inner(self) -> pthread_t { self.inner }
}

unsafe impl Send for ThreadHandle { }


fn client(server: ThreadHandle) {
    let mut counter = 1;
    let server = server.into_inner();
    loop {
        // dump value in UTCB
        unsafe {
            (*l4::l4_utcb_mr()).mr[0] = counter;
        }
        println!("client: value written to register");
        // To an L4 IPC call, i.e. send a message to thread2 and wait for a reply from thread2. The
        // '1' in the msgtag denotes that we want to transfer one word of our message registers
        // (i.e. MR0). No timeout.
        unsafe {
            let tag = l4::l4_ipc_call(l4::pthread_l4_cap(server), l4_utcb(),
                        l4_msgtag(0, 1, 0, 0),
                        timeout_never!());
            println!("data sent");
            // check for IPC error, if yes, print out the IPC error code, if not, print the received
            // result.
            match l4::l4_ipc_error(tag, l4_utcb()) {
                0 => // success
                    println!("Received: {}\n", (*l4::l4_utcb_mr()).mr[0]),
                ipc_error => println!("client: IPC error: {}\n",  ipc_error),
            };
        }
        thread::sleep(time::Duration::from_millis(1000));
        counter += 1;
    }
}

#[no_mangle]
pub extern "C" fn main() {
    let server = ThreadHandle { inner: unsafe { libc::pthread_self() as *mut ::std::os::raw::c_void } };
    let _client = thread::spawn(|| client(server));
    // turn thread handle into pthread_t:
    // https://doc.rust-lang.org/std/thread/struct.JoinHandle.html#impl-JoinHandleExt
    //  client.as_pthread_t()
    let mut label: l4_umword_t = unsafe { ::std::mem::uninitialized() };
    // square server
    // Wait for requests from any thread. No timeout, i.e. wait forever.
    println!("before waiting for incoming connections");
    let mut tag = unsafe { l4::l4_ipc_wait(l4::l4_utcb(), &mut label,
            timeout_never!()) };
    println!("waiting for incoming connections");
    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        unsafe {
            let ipc_error = l4::l4_ipc_error(tag, l4::l4_utcb());
            if ipc_error != 0 {
                println!("server: IPC error: {}\n", ipc_error);
                tag = l4::l4_ipc_wait(l4::l4_utcb(), &mut label, timeout_never!());
                continue;
            }
        }

        // the IPC was ok, now take the value out of message register 0 of the
        // UTCB and store the square of it back to it.
        unsafe {
            let val = (*l4::l4_utcb_mr()).mr[0];
            (*l4::l4_utcb_mr()).mr[0] = val * val;
            println!("new value: {}", (*l4::l4_utcb_mr()).mr[0]);

            // send reply and wait again for new messages.
            // The '1' in msgtag indicates that 1 word transfered from first
            // register
            tag = l4::l4_ipc_reply_and_wait(l4::l4_utcb(), l4_msgtag(0, 1, 0, 0),
                                  &mut label, timeout_never!());
        }
    }
}

/*
#[no_mangle]
pub extern "C" fn main() {
    println!("âye up");
}
*/
