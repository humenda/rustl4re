#![feature(libc)]
extern crate libc;
extern crate l4mem;
extern crate cap_sys as cap;
extern crate ipc_sys as ipc;

use ipc::{l4_ipc_error, l4_utcb};
use std::slice;
use std::os::raw::c_void;

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
        let rcv_ds = ds << l4mem::L4_CAP_SHIFT;
        (*ipc::l4_utcb_br()).br[0] = rcv_ds
                | cap::L4_RCV_ITEM_SINGLE_CAP as u64
                |  cap::L4_RCV_ITEM_LOCAL_ID as u64;
        (*ipc::l4_utcb_br()).br[1] = 0;
        (*ipc::l4_utcb_br()).bdr = 0;

        // open wait:
        let tag = ipc::l4_ipc_wait(l4_utcb(), &mut l,
                ipc::timeout_never());
        match ipc::l4_ipc_error(tag, l4_utcb()) {
            0 => (),
            x => {
                println!("IPC error {}", x);
                continue;
            }
        }
        if cap::l4_is_invalid_cap(ds) {
            println!("broken cap received");
        }

        let size_in_bytes = (*ipc::l4_utcb_mr()).mr[1];

        println!("First chars of message: {}",
                &(read_message(ds, size_in_bytes).unwrap())[..20]);
    }
}

unsafe fn read_message(mut ds_cap: l4mem::l4re_ds_t, length: u64)
        -> Result<String, String> {
    let virt_addr: *mut u8 = std::mem::uninitialized();
    // reserve area to allow attaching a data space; flags = 0 must be set
    println!("attaching memory, copying data");
    match l4mem::l4re_rm_attach(&mut (virt_addr as *mut c_void), length,
            l4mem::L4RE_RM_SEARCH_ADDR as u64, ds_cap, 0, l4mem::L4_PAGESHIFT) {
        0 => (),
        err => return Err(format!("unable to attach memory from dataspace: {}",
                err)),
    };
    // memcpy bytes into a vector
    let msg = Vec::from(slice::from_raw_parts(virt_addr, length as usize));
    let msg = String::from_utf8(msg);

    println!("detaching memory…");
    l4mem::l4re_rm_detach(virt_addr as *mut c_void, &mut ds_cap);
    msg.map_err(|e| e.to_string())
}
