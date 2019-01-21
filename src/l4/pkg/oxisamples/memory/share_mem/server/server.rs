#![feature(associated_type_defaults)]
extern crate l4_sys;
extern crate l4;
extern crate l4_derive;
extern crate l4re;

use l4_sys::*;
use l4::{error::Result,
    ipc,
    iface_enumerate, write_msg, iface_back, derive_ipc_calls,
};
use l4_derive::l4_callable;

iface_enumerate! {
    trait Shm {
        const PROTOCOL_ID: i64 = 0x42;
        type OpType = i32;
        fn witter(&mut self, length: u32, ds: ::l4::utcb::FlexPage) -> bool;
    }
    struct ShmClient;
}

#[l4_callable(demand = 1)]
struct ShmServer;

impl Shm for ShmServer {
    fn witter(&mut self, length: u32, ds: ::l4::utcb::FlexPage) -> Result<bool> {
        panic!("Not implemented!");
    }
}

fn main() {
    let chan = l4re::sys::l4re_env_get_cap("channel").expect(
            "Received invalid channel capability.");
    let mut srv_impl = ShmServer::new();
    let mut srv_loop = unsafe {
        ipc::LoopBuilder::new_at((*l4re::sys::l4re_env()).main_thread,
            l4_utcb())
        }.with_buffers()
        .build();
    srv_loop.register(chan, &mut srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}
