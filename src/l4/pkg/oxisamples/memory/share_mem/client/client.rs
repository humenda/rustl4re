extern crate l4_sys;
#[macro_use]
extern crate l4;
extern crate l4re;
extern crate libc; // only for C type definitions

use l4_sys::l4_cap_idx_t;
use l4::{
    cap,
    utcb::{FlexPage, FpageRights},
};
use l4re::{env,
    sys::l4re_rm_flags_t};
use std::{thread, time};
use std::mem;
use libc::c_void;

iface! {
    mod shm;

    trait Shm {
        const PROTOCOL_ID: i64 = 0x44;
        fn witter(&mut self, length: u32, ds: ::l4::utcb::FlexPage) -> bool;
    }
}

// C-style dataspace allocation
unsafe fn allocate_ds(size_in_bytes: usize)
        -> Result<(*mut c_void, l4_cap_idx_t), String> {
    // allocate ds capability slot
    let ds = l4_sys::l4re_util_cap_alloc();
    if l4_sys::l4_is_invalid_cap(ds) {
        return Err("Unable to acquire capability slot for l4re".into());
    }
    /*
    match l4re::sys::l4re_ds_allocate(ds, 0, size_in_bytes as u64) {
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
    match l4re::sys::l4re_ma_alloc(size_in_bytes as usize, ds, 0) {
        0 => (),
        code => return Err(format!(
                "Unable to allocate memory for l4re: {}", code)),
    };
    let mut stats = l4re::sys::l4re_ds_stats_t { size: 0, flags: 0 };
    l4re::sys::l4re_ds_info(ds, &mut stats);
    if (stats).size != l4_sys::l4_round_page(size_in_bytes) as u64 {
        panic!("memory allocation failed: got {:x}, required {:x}",
                 stats.size, l4_sys::l4_round_page(size_in_bytes));
    }

    // attach to region map
    let mut virt_addr: *mut c_void = mem::uninitialized();
    let r = l4re::sys::l4re_rm_attach(&mut virt_addr as *mut *mut c_void as _,
            size_in_bytes as u64, l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR as u64,
            ds, 0, l4_sys::L4_PAGESHIFT as u8);
    if r > 0 { // error, free ds
        l4re::sys::l4re_util_cap_free_um(ds);
        return Err(format!("Allocation failed with exit code {}", r));
    }
    Ok((virt_addr, ds))
}

unsafe fn free_ds(base: *mut c_void, ds: l4_cap_idx_t) -> Result<(), String> {
    // detach memory from our address space
    match l4re::sys::l4re_rm_detach(base as _) {
        0 => (),
        r => return Err(format!("Allocation failed with {}", r)),
    };
    // free memory at C allocator
    l4re::sys::l4re_util_cap_free_um(ds);
    Ok(())
}

pub fn main() {
    unsafe {
        unsafe_main();
    }
}

unsafe fn unsafe_main() {
    let server: cap::Cap<Witter> = env::get_cap("channel")
            .expect("Received invalid capability");
    if l4_sys::l4_is_invalid_cap(server) {
        panic!("No IPC Gate found.");
    }

    let text = (1..1000).fold(String::new(), |mut s, _| {
        s.push_str("Lots of text. ");
        s
    });
    let byteslice = text.as_bytes();
    let size_in_bytes = l4_sys::l4_round_page(text.as_bytes().len() + 1);
    let (base, ds) = allocate_ds(size_in_bytes as usize).unwrap();
    byteslice.as_ptr().copy_to(base as *mut u8, byteslice.len());
    *(base as *mut u8).offset(byteslice.len() as isize) = 0u8; // add 0-byte
    println!("allocated {:x} B for a string with {:x} B, sending capability",
            size_in_bytes, byteslice.len());

    // send blah blah using the generated client implementation
    let mut witter = Shm::client(server);
    witter.witter(byteslice.len() as u32 + 1,
            FlexPage::from_cap(cap::from(ds), FpageRights::RWX, None))
        .unwrap();

    free_ds(base, ds).unwrap();
    println!("sending successful");
    thread::sleep(time::Duration::new(2, 0));
    println!("bye");
}
