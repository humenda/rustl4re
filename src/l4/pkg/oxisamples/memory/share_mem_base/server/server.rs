extern crate libc;
extern crate l4re;
extern crate l4_sys as l4;

use l4::{l4_cap_idx_t, l4_ipc_error,
    l4_ipc_tcr_error_t::L4_IPC_ERROR_MASK,
    l4_msgtag,
    l4_msg_item_consts_t::*,
    l4_utcb, timeout_never};
use l4re::sys::{
    l4re_ds_info,
    l4re_ds_stats_t,
    l4re_ds_t,
    l4re_rm_attach,
    l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR,
    l4re_rm_detach,
    l4re_env_get_cap,
};
use libc::c_void;

#[inline]
unsafe fn setup_utcb(ds: &l4_cap_idx_t) {
    // create a receive item from a C cap. (instructing kernel where to map received capability
    // to)
    // (see l4sys/include/cxx/ipc_types.h and usage in moe//main.cc:setup_wait() and Small_buf
    // constructor (used in this location)
    (*l4::l4_utcb_br()).br[0] = ds | L4_RCV_ITEM_SINGLE_CAP as u64
            | L4_RCV_ITEM_LOCAL_ID as u64;
    (*l4::l4_utcb_br()).bdr = 0;
}


pub fn main() {
    unsafe { // safe nesting and unsafe, it's all unsafe, really
        unsafe_main()
    }
}

unsafe fn unsafe_main() {
    let gate = l4re_env_get_cap("channel")
            .expect("Received invalid IPC gate capability.");
    if l4::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }
    match l4_ipc_error(l4::l4_rcv_ep_bind_thread(gate,
            (*l4re::sys::l4re_env()).main_thread, 0b111100), l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");
    // location where kernel writes sender label to
    let mut l = 0 as l4::l4_umword_t;
    let mut ds = l4::l4re_util_cap_alloc(); // new cap slot

    setup_utcb(&mut ds); // prepare for receiving cap
    // open wait:
    let mut tag = l4::l4_ipc_wait(l4_utcb(), &mut l, l4::timeout_never());
    loop {
        match l4::l4_ipc_error(tag, l4_utcb()) {
            0 => (),
            x => panic!("IPC error {}", x),
        };

        if l4::l4_is_invalid_cap(ds) {
            panic!("broken cap received");
        }

        let size_in_bytes = (*l4::l4_utcb_mr()).mr[0] as usize;

        let msg = read_message(ds, size_in_bytes).unwrap();
        { // print first 20 characters; a bit tricky, because a bit is not a character, so indexing is more complicated
            let (i, c) = msg.char_indices().nth(20 - 1).unwrap();
            println!("First chars of message: {}…", &msg[0..(i + c.len_utf8())]);
        }
        l4::l4re_util_cap_free(ds);
        setup_utcb(&mut ds);
        ds = l4::l4re_util_cap_alloc(); // new cap slot
        tag = l4::l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 0, 0, 0), &mut l, timeout_never());
    }
}

unsafe fn read_message(ds_cap: l4re_ds_t, length: usize)
        -> Result<String, String> {
    let mut stats: l4re_ds_stats_t = std::mem::uninitialized();
    match l4re_ds_info(ds_cap, &mut stats as *mut l4re_ds_stats_t) {
        0 => (),
        n => panic!("stats info gave error {}\n",
                    n & (L4_IPC_ERROR_MASK as i32)), 
    };
    if (stats).size != l4::l4_round_page(length) as u64 {
        panic!("memory allocation failed: got {:x}, required {:x}",
                 stats.size, l4::l4_round_page(length));
    }

    let mut virt_addr: *mut c_void = std::mem::uninitialized();
    // reserve area to allow attaching a data space; flags = 0 must be set
    let page_size = l4::l4_round_page(length);
    println!("attaching memory: {:x} bytes", page_size);
    match l4re_rm_attach((&mut virt_addr) as *mut *mut c_void as _, page_size,
            L4RE_RM_SEARCH_ADDR as u64, ds_cap, 0, l4::L4_PAGESHIFT as u8) {
        0 => (),
        err => return Err(format!("unable to attach memory from dataspace: {}",
                err)),
    };
    println!("copying string, length: {:x}; base: {:p}", length, virt_addr);
    // copy bytes into a vector
    let mut msg: Vec<u8> = Vec::with_capacity(length as usize);
    msg.set_len(length);
    (virt_addr as *mut u8).copy_to(msg.as_mut_ptr(), length);
    println!("copied");
    let msg = String::from_utf8(msg);

    println!("detaching memory…");
    l4re_rm_detach(virt_addr as _);
    msg.map_err(|e| e.to_string())
}
