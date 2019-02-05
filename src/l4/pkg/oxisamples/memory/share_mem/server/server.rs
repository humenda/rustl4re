#![feature(associated_type_defaults)]
extern crate l4_sys;
extern crate l4;
extern crate l4_derive;
extern crate l4re;

use l4_sys::*;
use l4::{error::Result,
    ipc,
    cap::{Cap, Untyped},
};
use l4_derive::{iface, l4_server};
use l4re::OwnedCap;

// include IPC interface definition
include!("../interface.rs");

#[l4_server(demand = 1)]
struct ShmServer;

impl Shm for ShmServer {
    fn witter(&mut self, length: u32, ds: Cap<Untyped>) -> Result<bool> {
        let ds = OwnedCap::from(ds)?;
        panic!("Not implemented!");
    }
}

unsafe fn read_message(ds: Cap<Untyped>, length: usize)
        -> core::result::Result<String, String> {
    use l4re::sys::{l4re_ds_stats_t, l4re_ds_info};
    use l4::{cap::Interface, sys::l4_ipc_tcr_error_t::L4_IPC_ERROR_MASK};
    use core::ffi::c_void;
    let mut stats: l4re_ds_stats_t = std::mem::uninitialized();
    match l4re_ds_info(ds.cap(), &mut stats as *mut l4re_ds_stats_t) {
        0 => (),
        n => panic!("stats info gave error {}\n",
                    n & (L4_IPC_ERROR_MASK as i32)), 
    };
    if (stats).size != l4::sys::l4_round_page(length) as u64 {
        panic!("memory allocation failed: got {:x}, required {:x}",
                 stats.size, l4::sys::l4_round_page(length));
    }

    let mut virt_addr: *mut c_void = std::mem::uninitialized();
    // reserve area to allow attaching a data space; flags = 0 must be set
    let page_size = l4::sys::l4_round_page(length);
    println!("attaching memory: {:x} bytes", page_size);
    match l4re::sys::l4re_rm_attach((&mut virt_addr) as *mut *mut c_void as _, page_size,
            l4re::sys::l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR as u64, ds.cap(), 0, l4::sys::L4_PAGESHIFT as u8) {
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
    l4re::sys::l4re_rm_detach(virt_addr as _);
    msg.map_err(|e| e.to_string())
}
fn main() {
    let chan = l4re::sys::l4re_env_get_cap("channel").expect(
            "Received invalid channel capability.");
    let mut srv_impl = ShmServer::new(chan);
    let mut srv_loop = unsafe {
        ipc::LoopBuilder::new_at((*l4re::sys::l4re_env()).main_thread,
            l4_utcb())
        }.with_buffers()
        .build();
    srv_loop.register(&mut srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}
