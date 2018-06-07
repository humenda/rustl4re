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
            (*cap::l4re_env()).main_thread, 0b111100), l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");
    loop {
        let ds = cap::l4re_util_cap_alloc();
        let mut l: ipc::l4_umword_t = ::std::mem::uninitialized();
        // create a receive item from a C cap. (instructing kernel where to map received capability
        // to)
        // (see l4sys/include/cxx/ipc_types.h and usage in moe//main.cc:setup_wait() and Small_buf
        // constructor (used in this location)
        let rcv_ds = ds << cap::l4_cap_consts_t_L4_CAP_SHIFT;
        (*ipc::l4_utcb_br()).br[0] = rcv_ds
                | cap::l4_msg_item_consts_t_L4_RCV_ITEM_SINGLE_CAP as u64
                |  cap::l4_msg_item_consts_t_L4_RCV_ITEM_LOCAL_ID as u64;
        (*ipc::l4_utcb_br()).br[1] = 0;
        (*ipc::l4_utcb_br()).bdr = 0;

        // open wait:
        let tag = ipc::l4_ipc_wait(l4_utcb(), &mut l,
                ipc::timeout_never());
        let x = ipc::l4_ipc_error(tag, l4_utcb());
        if x != 0 {
            println!("IPC error {}", x);
            continue;
        }
        if cap::l4_is_invalid_cap(ds) {
            println!("broken cap received");
        } else {
            println!("received good cap, ToDo");
        }
    }
}

