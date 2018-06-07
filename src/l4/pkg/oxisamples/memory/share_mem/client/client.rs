extern crate l4mem;
extern crate cap_sys as cap;
extern crate ipc_sys as ipc;

use cap::l4_cap_idx_t;
use ipc::l4_utcb;
use std::ptr;
use std::mem;
use std::os::raw::c_void;

/// This in fact matches roughly the C example (ma+rm.c), also offering a alloc_mem (→new) and a
/// free (→drop) function. We like being safe, so "waste" another three lines to wrap the thing in
/// a struct.
struct Memory {
    pub base: *mut ::std::os::raw::c_void,
    pub ds: l4mem::l4re_ds_t, // type alias l4_cap_idx_t
}

impl Memory {
    fn new(size_in_bytes: usize) -> Result<Memory, String> {
        // allocate ds capability slot
        let ds = unsafe {
            cap::l4re_util_cap_alloc()
        };
        if cap::l4_is_invalid_cap(ds) {
            return Err("Unable to acquire capability slot for l4mem".into());
        }

        // map memory into ds
        match unsafe { l4mem::l4re_ma_alloc(size_in_bytes, ds, 0) } {
            0 => (),
            code => panic!("Unable to allocate memory for l4mem: {}", code),
        }

        unsafe {
            match l4mem::l4re_ds_allocate(ds, 0, size_in_bytes as u64) {
                0 => (),
                n if n == -(cap::L4_ERANGE as i64) => panic!("Given range outside l4mem"),
                n if n == -(cap::L4_ENOMEM as i64) => panic!("Not enough memory available"),
                n if n <= (-(cap::L4_EIPC_LO as i64)) &&
                    n >= (-(cap::L4_EIPC_HI as i64)) => panic!(
                        "IPC error: {}", n),
                r => panic!("Error while mapping memory into ds: {}", r),
            }
        }


        // attach to region map
        unsafe {
            let mut virt_addr: *mut c_void = mem::uninitialized();
            let r = l4mem::l4re_rm_attach(&mut virt_addr, size_in_bytes as u64,
                                              l4mem::L4RE_RM_SEARCH_ADDR as u64, ds, 0,
                                              l4mem::L4_PAGESHIFT);
            if r > 0 { // error, free ds
                l4mem::l4re_util_cap_free_um(ds);
                return Err(format!("Allocation failed with exit code {}", r));
            }
            return Ok(Memory { base: virt_addr, ds: ds })
        }
    }
}

impl Drop for Memory {
    fn drop(&mut self) {
        unsafe {
            let mut ds: l4mem::l4re_ds_t = ::std::mem::uninitialized();

            // detach memory from our address space
            match l4mem::l4re_rm_detach_ds(self.base, &mut ds) {
                0 => (),
                r => panic!(format!("Allocation failed with {}", r)),
            };
            // free memory at C allocator
            l4mem::l4re_util_cap_free_um(ds);
        }
    }
}

pub unsafe fn send_cap(payload: l4_cap_idx_t, size: usize, dst: l4_cap_idx_t)
        -> Result<(), String> {
    let mr = ipc::l4_utcb_mr();
    println!("Index of cap to send: {:x}, destination: {:x}", payload, dst);
    (*mr).mr[0] = cap::l4_obj_fpage(payload, 0, cap::L4_FPAGE_RWX as u8).raw;
    (*mr).mr[1] = size as u64;
    (*mr).mr[2] = 0;
    match ipc::l4_ipc_error(ipc::l4_ipc_call(dst, l4_utcb(),
            ipc::l4_msgtag(0, 2, 1, 0), ipc::l4_timeout_t { raw: 0 }), l4_utcb()) {
        0 => Ok(()),
        n => Err(format!("IPC error while sending capability: {}", n)),
    }
}


#[no_mangle]
pub extern "C" fn main() {
    let server = unsafe { cap::l4re_env_get_cap("channel") };
    if cap::l4_is_invalid_cap(server) {
        panic!("No IPC Gate found.");
    }

    let text = (1..1000).fold(String::new(), |mut s, _| {
        s.push_str("Lots of text. ");
        s
    });
    let size_in_bytes = l4mem::required_pages(text.len());
    // ok, this is absolutely unsafe: no size check, no proper "drop" handling, so a potential
    // resource leak. But I think you get it.
    let mmem = Memory::new(size_in_bytes).unwrap();
    unsafe {
        ptr::copy_nonoverlapping(text.as_str().as_ptr(), mmem.base as *mut u8, text.len());
    }
    println!("[client]: memory allocated, about to send data space cap");

    unsafe {
        send_cap(mmem.ds, size_in_bytes, server).unwrap();
    }
    println!("[client]: sending successful");
}
