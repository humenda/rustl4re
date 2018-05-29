#![feature(libc)]
extern crate libc;
extern crate l4mem;
extern crate cap_sys as cap;
extern crate ipc_sys as ipc;

use ipc::{l4_ipc_error, l4_utcb};

#[no_mangle]
pub extern "C" fn main() {
    unsafe { // safe nesting and unsafe, it's all unsafe, really
        unsafe_main()
    }
}

unsafe fn unsafe_main() {
    let gate = cap::l4re_env_get_cap("channel");
    if cap::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }
    match l4_ipc_error(ipc::l4_rcv_ep_bind_thread(gate,
            (*cap::l4re_env()).main_thread, 0xdead), l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");
    loop {
        let ds = cap::l4re_util_cap_alloc();
        let mut l: ipc::l4_umword_t = ::std::mem::uninitialized();
        println!("[server]: waiting for incoming connections");
        // open wait:
        let tag = ipc::l4_ipc_wait(l4_utcb(), &mut l,
                ipc::timeout_never());
        println!("frrrkrkrk");
        if ipc::l4_msgtag_has_error(tag) {
            println!("IPC error, not telling you which!");
            continue;
        }
        if cap::l4_is_invalid_cap(ds) {
            println!("broken cap received");
        } else {
            println!("received good cap, ToDo");
        }
    }
}

