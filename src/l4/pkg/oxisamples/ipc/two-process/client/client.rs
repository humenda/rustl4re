extern crate ipc_sys as ipc;
extern crate cap_sys as cap;

use ipc::{l4_utcb, l4_msgtag};
use std::{thread, time};

#[no_mangle]
pub extern "C" fn main() {
    let server = unsafe { cap::l4re_env_get_cap("echo_server") };
    if cap::l4_is_invalid_cap(server) {
        println!("No IPC Gate found.");
    }
    let mut counter = 1;
    loop {
        // dump value in UTCB
        unsafe {
            (*ipc::l4_utcb_mr()).mr[0] = counter;
        }
        println!("client: value written to register");
        // To an L4 IPC call, i.e. send a message to thread2 and wait for a reply from thread2. The
        // '1' in the msgtag denotes that we want to transfer one word of our message registers
        // (i.e. MR0). No timeout.
        unsafe {
            let tag = ipc::l4_ipc_call(server, l4_utcb(),
                        l4_msgtag(0, 1, 0, 0),
                        ipc::l4_timeout_t { raw: 0 });
            println!("data sent");
            // check for IPC error, if yes, print out the IPC error code, if not, print the received
            // result.
            match ipc::l4_ipc_error(tag, l4_utcb()) {
                0 => // success
                    println!("Received: {}\n", (*ipc::l4_utcb_mr()).mr[0]),
                ipc_error => println!("client: IPC error: {}\n",  ipc_error),
            };
        }
        thread::sleep(time::Duration::from_millis(1000));
        counter += 1;
    }
}


