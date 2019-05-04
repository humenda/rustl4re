extern crate core;
extern crate l4_sys;
extern crate l4_derive;
extern crate l4;
extern crate l4re;

use l4::{error::Result, ipc};
use l4_derive::{iface, l4_server};
use l4_sys::{l4_utcb};

// include shared interface definition (located relative to the directory of
// this file)
include!("../../interface.rs");

#[l4_server(Calculator)]
struct CalcServer;

impl Calculator for CalcServer {
    fn sub(&mut self, a: u32, b: u32) -> Result<i32> {
        let x = a as i32 - b as i32;
        Ok(x)
    }

    fn neg(&mut self, a: u32) -> Result<i32> {
        Ok(a as i32 * -1)
    }
}

fn main() {
    println!("Calculation server starting…");
    let chan = l4re::sys::l4re_env_get_cap("calc_server").expect(
            "Received invalid capability for calculation server.");
    let mut srv_impl = CalcServer::new(chan);
    let mut srv_loop = unsafe {
        ipc::LoopBuilder::new_at((*l4re::sys::l4re_env()).main_thread,
            l4_utcb())
    }.build();
    srv_loop.register(&mut srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}
