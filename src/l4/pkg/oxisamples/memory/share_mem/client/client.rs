#![feature(associated_type_defaults)]
extern crate l4_derive;
extern crate l4;
extern crate l4re;
extern crate libc; // only for C type definitions

use l4_derive::{iface, l4_client};
use l4::sys::l4_cap_idx_t;
use l4::cap::{Cap, Interface};
use l4re::{env,
    mem::DataspaceProvider,
    OwnedCap,
    sys::l4re_rm_flags_t};
use std::{thread, time};
use std::mem;
use libc::c_void;

// include IPC interface definition
include!("../interface.rs");

#[l4_client(Shm)]
struct ShmClient;

// C-style dataspace allocation
unsafe fn allocate_ds(size_in_bytes: usize)
        -> Result<(*mut c_void, OwnedCap<Dataspace>), String> {
    // allocate ds capability slot
    let mut ds = OwnedCap::<Dataspace>::alloc();
    if ds.is_invalid() {
        return Err("Unable to acquire capability slot for l4re".into());
    }
    /*
    match l4re::sys::l4re_ds_allocate(ds, 0, size_in_bytes as u64) {
        0 => (),
        n if n == -(l4::sys::L4_ERANGE as i64) => panic!("Given range outside l4re"),
        n if n == -(l4::sys::L4_ENOMEM as i64) => panic!("Not enough memory available"),
        n => {
            let n = l4::sys::ipc_error2code(n as isize);
            panic!("[err {}]: failed to allocate {:x} bytes for a dataspace with cap index {:x}",
                n, size_in_bytes, ds);
        }
    }
    */
    // map memory into ds
    match l4re::sys::l4re_ma_alloc(size_in_bytes as usize, ds.raw(), 0) {
        0 => (),
        code => return Err(format!(
                "Unable to allocate memory for l4re: {}", code)),
    };
    let stats = ds.info().map_err(|e| e.to_string())?;
    if stats.size != l4::round_page(size_in_bytes) as u64 {
        panic!("memory allocation failed: got {:x}, required {:x}",
                 stats.size, l4::round_page(size_in_bytes));
    }

    // attach to region map
    let mut virt_addr: *mut c_void = mem::uninitialized();
    let r = l4re::sys::l4re_rm_attach(&mut virt_addr as *mut *mut c_void as _,
            size_in_bytes as u64, l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR as u64,
            ds.raw(), 0, l4::sys::L4_PAGESHIFT as u8);
    if r > 0 { // error, free ds
        drop(ds);
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
    // free capability index (from capability allocator)
    l4re::sys::l4re_util_cap_free_um(ds);
    Ok(())
}

pub fn main() {
    unsafe {
        unsafe_main();
    }
}

unsafe fn unsafe_main() {
    println!("good morning sir");
    let mut server: Cap<ShmClient> = env::get_cap("channel")
            .expect("Received invalid capability");
    if server.is_invalid() {
        panic!("No IPC Gate found.");
    }

    let text = (1..1000).fold(String::new(), |mut s, _| {
        s.push_str("Lots of text. ");
        s
    });
    let byteslice = text.as_bytes();
    let size_in_bytes = l4::round_page(text.as_bytes().len() + 1);
    let (base, ds) = allocate_ds(size_in_bytes as usize).unwrap();
    byteslice.as_ptr().copy_to(base as *mut u8, byteslice.len());
    *(base as *mut u8).offset(byteslice.len() as isize) = 0u8; // add 0-byte
    println!("allocated {:x} B for a string with {:x} B, sending capability index 0x{:x}",
            size_in_bytes, byteslice.len(), ds.raw());

    // send blah blah using the generated client implementation
    server.witter(byteslice.len() as u32 + 1, ds.cap())
        .unwrap();

    free_ds(base, ds.raw()).unwrap();
    println!("sending successful");
    thread::sleep(time::Duration::new(2, 0));
    println!("bye");
}
