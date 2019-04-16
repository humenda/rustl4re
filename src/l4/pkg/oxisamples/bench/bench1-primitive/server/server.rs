#![feature(associated_type_defaults)]
extern crate core;
extern crate l4_sys;
extern crate l4_derive;
extern crate l4;
extern crate l4re;

use l4::{error::Result, ipc};
use l4_derive::{iface, l4_server};
use l4_sys::{l4_utcb};
#[cfg(bench_serialisation)]
use l4re::{OwnedCap,
    mem::Dataspace};

// include shared interface definition (located relative to the directory of
// this file)
include!("../../interface.rs");

#[l4_server(Bencher)]
struct BenchServer;

impl Bencher for BenchServer {
    fn sub(&mut self, a: u32, b: u32) -> Result<i32> {
        let x = a as i32 - b as i32;
        Ok(x)
    }

    fn str2leet(&mut self, a: &str) -> Result<String> {
        Ok(a.chars().map(|c| match c as char {
                't' => '7',
                'a' => '4',
                'e' => '3',
                'o' => '0',
                'l' => '1',
                's' => '5',
                k => k,
            }).collect())
    }
}

// set up shared memory for collection measurements from the server
#[cfg(bench_serialisation)]
fn setup_shm() -> OwnedCap<Dataspace> {
    use l4::sys::*;
    let ds = OwnedCap::<Dataspace>::alloc();
    unsafe {
        let br: *mut l4_buf_regs_t = l4_utcb_br();
        (*br).bdr = 0;
        (*br).br[0] = ds.raw()
                | l4_msg_item_consts_t::L4_RCV_ITEM_SINGLE_CAP as u64
                | l4_msg_item_consts_t::L4_RCV_ITEM_LOCAL_ID as u64;
    }
    let size = unsafe {
        let mut label = 0;
        // gate was registered with loop.register
        let tag = l4::ipc::MsgTag::from(l4_ipc_wait(l4_utcb(), &mut label,
                timeout_never())).result().unwrap();
        if tag.words() != 1 {
            panic!("Err, received {} words and {} items", tag.words(), tag.items());
        }
        (*l4_utcb_mr()).mr[0]
    };
    let mut ds_start: *mut libc::c_void = 0 as *mut _;
    // attach received dataspace to virtual memory region and let the
    // measurement functions point to it
    unsafe {
        let err = l4re::sys::l4re_rm_attach(
                (&mut ds_start) as *mut *mut _, size,
                l4re::sys::l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR as u64,
                ds.raw(), 0, l4::sys::L4_PAGESHIFT as u8);
        if err != 0 {
            println!("error while attaching memory: {}", err);
        }
       l4::SERVER_MEASUREMENTS = ds_start as *mut l4::Measurements<l4::ServerDispatch>;
    }
    ds
}

fn main() {
    println!("Starting benchmark server");
    let chan = l4re::env::get_cap::<l4::cap::Untyped>("channel").expect(
            "Received invalid capability for benchmark server.");
    let mut srv_impl = BenchServer::new(unsafe { chan.raw() });
    let mut srv_loop = unsafe {
        ipc::LoopBuilder::new_at((*l4re::sys::l4re_env()).main_thread,
            l4_utcb())
    }.build();
    srv_loop.register(&mut srv_impl).expect("Failed to register server in loop");
    #[cfg(bench_serialisation)]
    let _ds = setup_shm(); // needs to be there to be dropped later on

    println!("Waiting for incoming connections");
    srv_loop.start();
}
