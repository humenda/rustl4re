#![feature(libc)]
extern crate libc;
extern crate l4re_sys as l4re;
extern crate l4_sys as l4;

use l4::l4_cap_idx_t;
use l4::{l4_ipc_error, l4_msgtag, l4_utcb, timeout_never};
use std::slice;
use std::os::raw::c_void;

#[inline]
unsafe fn setup_utcb(ds: &l4_cap_idx_t) {
    // create a receive item from a C cap. (instructing kernel where to map received capability
    // to)
    // (see l4sys/include/cxx/ipc_types.h and usage in moe//main.cc:setup_wait() and Small_buf
    // constructor (used in this location)
    let rcv_ds = ds << l4re::L4_CAP_SHIFT;
    (*l4::l4_utcb_br()).br[0] = rcv_ds | l4::L4_RCV_ITEM_SINGLE_CAP as u64
            |  l4::L4_RCV_ITEM_LOCAL_ID as u64;
    (*l4::l4_utcb_br()).br[1] = 0;
    (*l4::l4_utcb_br()).bdr = 0;
}


#[no_mangle]
pub extern "C" fn main() {
    unsafe { // safe nesting and unsafe, it's all unsafe, really
        unsafe_main()
    }
}

unsafe fn unsafe_main() {
    let gate = l4::l4re_env_get_cap("channel");
    if l4::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }
    match l4_ipc_error(l4::l4_rcv_ep_bind_thread(gate,
            (*l4::l4re_env()).main_thread, 0b111100), l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");
    // location where kernel writes sender label to
    let mut l: l4::l4_umword_t = ::std::mem::uninitialized();
    let mut ds = l4::l4re_util_cap_alloc(); // new cap slot

    setup_utcb(&mut ds); // prepare for receiving cap
    let mut tag = std::mem::uninitialized();
    // open wait:
    tag = l4::l4_ipc_wait(l4_utcb(), &mut l, l4::timeout_never());
    loop {
        match l4::l4_ipc_error(tag, l4_utcb()) {
            0 => (),
            x => panic!("IPC error {}", x),
        };

        if l4::l4_is_invalid_cap(ds) {
            panic!("broken cap received");
        }

        let size_in_bytes = (*l4::l4_utcb_mr()).mr[1] as usize;

        let msg = read_message(ds, size_in_bytes).unwrap();
        { // print first 20 characters; a bit tricky, because a bit is not a character, so indexing is more complicated
            let (i, c) = msg.char_indices().nth(20 - 1).unwrap();
            println!("First chars of message: {}", &msg[0..(i + c.len_utf8())]);
        }
        l4::l4re_util_cap_free(ds);
        // ToDo how to set up UTCB for reply *and* wait?
        setup_utcb(&mut ds);
        ds = l4::l4re_util_cap_alloc(); // new cap slot
        tag = l4::l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 0, 0, 0), &mut l, timeout_never());
    }
}

unsafe fn read_message(mut ds_cap: l4re::l4re_ds_t, length: usize)
        -> Result<String, String> {
    let virt_addr: *mut u8 = std::mem::uninitialized();
    // reserve area to allow attaching a data space; flags = 0 must be set
    let page_size = l4re::l4_round_page(length);
    println!("attaching memory: {:x} bytes", page_size);
    match l4re::l4re_rm_attach(&mut (virt_addr as *mut c_void), page_size,
            l4re::L4RE_RM_SEARCH_ADDR as u64, ds_cap, 0, l4re::L4_PAGESHIFT) {
        0 => (),
        err => return Err(format!("unable to attach memory from dataspace: {}",
                err)),
    };
    println!("copying string, length: {:x}; base: {:p}", length, virt_addr);
    // memcpy bytes into a vector
    let mut msg = Vec::with_capacity(length as usize + 1usize); // \0
    msg.append(&mut Vec::from(slice::from_raw_parts(virt_addr, length as usize)));
    //let msg = Vec::from(slice::from_raw_parts(virt_addr, length as usize));
    println!("copied");
    let msg = String::from_utf8(msg);

    println!("detaching memory…");
    //println!("err code: {:?}", CStr::from_ptr(l4::l4sys_errtostr(-2004)));
    l4re::l4re_rm_detach_ds(virt_addr as *mut c_void, &mut ds_cap);
    msg.map_err(|e| e.to_string())
}
