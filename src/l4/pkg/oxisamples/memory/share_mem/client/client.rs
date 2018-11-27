extern crate l4_sys;
#[macro_use]
extern crate l4;
extern crate l4re_sys as l4re;
extern crate libc; // only for C type definitions

use l4_sys::{l4_cap_idx_t, l4_utcb,
    L4_fpage_rights::L4_FPAGE_RWX,
    l4_msg_item_consts_t::L4_ITEM_MAP,
};
use l4::{
    cap::SendFpage,
    utcb::Msg,
};
use l4re::l4re_rm_flags_t;
use std::{thread, time};
use std::mem;
use libc::c_void;

unsafe fn allocate_ds(size_in_bytes: usize)
        -> Result<(*mut c_void, l4_cap_idx_t), String> {
    // allocate ds capability slot
    let ds = l4_sys::l4re_util_cap_alloc();
    if l4_sys::l4_is_invalid_cap(ds) {
        return Err("Unable to acquire capability slot for l4re".into());
    }
    /*
    match l4re::l4re_ds_allocate(ds, 0, size_in_bytes as u64) {
        0 => (),
        n if n == -(l4_sys::L4_ERANGE as i64) => panic!("Given range outside l4re"),
        n if n == -(l4_sys::L4_ENOMEM as i64) => panic!("Not enough memory available"),
        n => {
            let n = l4_sys::ipc_error2code(n as isize);
            panic!("[err {}]: failed to allocate {:x} bytes for a dataspace with cap index {:x}",
                n, size_in_bytes, ds);
        }
    }
    */
    // map memory into ds
    match l4re::l4re_ma_alloc(size_in_bytes as usize, ds, 0) {
        0 => (),
        code => return Err(format!(
                "Unable to allocate memory for l4re: {}", code)),
    };
    let mut stats = l4re::l4re_ds_stats_t { size: 0, flags: 0 };
    l4re::l4re_ds_info(ds, &mut stats);
    if (stats).size != l4re::l4_round_page(size_in_bytes) as u64 {
        panic!("memory allocation failed: got {:x}, required {:x}",
                 stats.size, l4re::l4_round_page(size_in_bytes));
    }

    // attach to region map
    let mut virt_addr: *mut c_void = mem::uninitialized();
    let r = l4re::l4re_rm_attach(&mut virt_addr, size_in_bytes as u64,
              l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR as u64, ds, 0,
              l4_sys::L4_PAGESHIFT as u8);
    if r > 0 { // error, free ds
        l4re::l4re_util_cap_free_um(ds);
        return Err(format!("Allocation failed with exit code {}", r));
    }
    Ok((virt_addr, ds))
}

unsafe fn free_ds(base: *mut c_void, ds: l4_cap_idx_t) -> Result<(), String> {
    // detach memory from our address space
    match l4re::l4re_rm_detach(base) {
        0 => (),
        r => return Err(format!("Allocation failed with {}", r)),
    };
    // free memory at C allocator
    l4re::l4re_util_cap_free_um(ds);
    Ok(())
}

unsafe fn send_cap(ds: l4_cap_idx_t, textlen: usize, dst: l4_cap_idx_t)
        -> Result<(), String> {
    let mut msg = Msg::new(); // get access to the message registers
    println!("cap info: index {:x}, destination: {:x}, memory size: {:x}",
             ds, dst, textlen);
    msg.write(0u64).unwrap(); // OpCode
    // msg.write(0u64);
    // if flex pages and data words are sent, flex pages need to be the last
    // items
    msg.write(textlen as u64).unwrap();
    // sending a capability requires to registers, one containing the action,
    // the other the flexpage
    // ToDo: use Cap interface when passing to function
    let rfp = SendFpage::new(l4_sys::l4_obj_fpage(ds, 0, L4_FPAGE_RWX as
                  u8).raw,
          0, None);
    msg.write(rfp).unwrap();
    match l4_sys::l4_ipc_error(l4_sys::l4_ipc_call(dst, l4_utcb(),
            l4_sys::l4_msgtag(0x44, 2, 1, 0), l4_sys::l4_timeout_t { raw: 0 }), l4_utcb()) {
        0 => Ok(()),
        n => Err(format!("IPC error while sending capability: {}", n)),
    }
}

pub fn main() {
    unsafe {
        unsafe_main();
    }
}

unsafe fn unsafe_main() {
    let server = l4re::l4re_env_get_cap("channel")
            .expect("Received invalid capability");
    if l4_sys::l4_is_invalid_cap(server) {
        panic!("No IPC Gate found.");
    }

    let text = (1..1000).fold(String::new(), |mut s, _| {
        s.push_str("Lots of text. ");
        s
    });
    let size_in_bytes = l4re::l4_round_page(text.as_bytes().len());
    let (base, ds) = allocate_ds(size_in_bytes as usize).unwrap();
    let byteslice = text.as_bytes();
    byteslice.as_ptr().copy_to(base as *mut u8, byteslice.len());
    println!("allocated {:x} B for a string with {:x} B, sending capability",
            size_in_bytes, byteslice.len());

    send_cap(ds, byteslice.len(), server).unwrap();
    free_ds(base, ds).unwrap();
    println!("sending successful");
    thread::sleep(time::Duration::new(2, 0));
    println!("bye");
}
