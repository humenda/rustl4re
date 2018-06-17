extern crate l4re_sys as l4re;

use l4::l4_cap_idx_t;
use l4::l4_utcb;
use l4re::l4_addr_t;
use std::{thread, time};
use std::mem;
use std::os::raw::c_void;

unsafe fn allocate_ds(size_in_bytes: l4_addr_t)
        -> Result<(*mut c_void, l4_cap_idx_t), String> {
    // allocate ds capability slot
    let ds = l4::l4re_util_cap_alloc();
    if l4::l4_is_invalid_cap(ds) {
        return Err("Unable to acquire capability slot for l4re".into());
    }
    match l4re::l4re_ds_allocate(ds, 0, size_in_bytes as u64) {
        0 => (),
        n if n == -(l4::L4_ERANGE as i64) => panic!("Given range outside l4re"),
        n if n == -(l4::L4_ENOMEM as i64) => panic!("Not enough memory available"),
        n => {
            let n = l4::ipc_error2code(n as isize);
            panic!("[err {}]: failed to allocate {:x} bytes for a dataspace with cap index {:x}",
                n, size_in_bytes, ds);
        }
    }
    // map memory into ds
    match l4re::l4re_ma_alloc(size_in_bytes as usize, ds, 0) {
        0 => (),
        code => return Err(format!(
                "Unable to allocate memory for l4re: {}", code)),
    }
    let stats: *mut l4re::l4re_ds_stats_t = std::mem::uninitialized();
    l4re::l4re_ds_info(ds, stats);
    if (*stats).size != l4re::l4_round_page(size_in_bytes) as u64 {
        panic!("memory allocation failed: got {:x}, required {:x}",
                 (*stats).size, l4re::l4_round_page(size_in_bytes));
    }


    // attach to region map
    let mut virt_addr: *mut c_void = mem::uninitialized();
    let r = l4re::l4re_rm_attach(&mut virt_addr, size_in_bytes,
              l4re::L4RE_RM_SEARCH_ADDR as u64, ds, 0, l4re::L4_PAGESHIFT);
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
    let mr = l4::l4_utcb_mr();
    println!("cap info: index {:x}, destination: {:x}, memory size: {:x}",
             ds, dst, textlen);
    (*mr).mr[0] = l4::l4_obj_fpage(ds, 0, l4::L4_FPAGE_RWX as u8).raw;
    (*mr).mr[1] = textlen as u64;
    (*mr).mr[2] = 0;
    match l4::l4_ipc_error(l4::l4_ipc_call(dst, l4_utcb(),
            l4::l4_msgtag(0, 2, 1, 0), l4::l4_timeout_t { raw: 0 }), l4_utcb()) {
        0 => Ok(()),
        n => Err(format!("IPC error while sending capability: {}", n)),
    }
}

#[no_mangle]
pub extern "C" fn main() {
    unsafe {
        unsafe_main();
    }
}

unsafe fn unsafe_main() {
    let server = l4::l4re_env_get_cap("channel");
    if l4::l4_is_invalid_cap(server) {
        panic!("No IPC Gate found.");
    }

    let text = (1..1000).fold(String::new(), |mut s, _| {
        s.push_str("Lots of text. ");
        s
    });
    let size_in_bytes = l4re::l4_round_page(text.as_bytes().len());
    let (base, ds) = allocate_ds(size_in_bytes).unwrap();
    let byteslice = text.as_bytes();
    byteslice.as_ptr().copy_to(base as *mut u8, byteslice.len());
    println!("allocated {:x} B for a string with {:x} B, sending capability",
            byteslice.len(), size_in_bytes);

    send_cap(ds, byteslice.len(), server).unwrap();
    free_ds(base, ds).unwrap();
    println!("[client]: sending successful");
    thread::sleep(time::Duration::new(2, 0));
    println!("bye");
}
