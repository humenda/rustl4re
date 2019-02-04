#![feature(associated_type_defaults)]
extern crate l4_sys;
extern crate l4;
extern crate l4_derive;
extern crate l4re;

use l4_sys::*;
use l4::{error::Result,
    ipc,
    cap::{self, Cap, Untyped},
};
use l4_derive::{iface, l4_server};

iface! {
    trait Shm {
        const PROTOCOL_ID: i64 = 0x42;
        fn witter(&mut self, length: u32, ds: Cap<Untyped>) -> bool;
    }
}

#[l4_server(demand = 1)]
struct ShmServer;

impl Shm for ShmServer {
    fn witter(&mut self, length: u32, ds: Cap<Untyped>) -> Result<bool> {
        let ds = unsafe {
            let mut c = cap::from(l4re::sys::l4re_util_cap_alloc());
            c.transfer(ds)?;
            c // ^ move cap index from loop receive slot into own binding
        };
        panic!("Not implemented!");
    }
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
