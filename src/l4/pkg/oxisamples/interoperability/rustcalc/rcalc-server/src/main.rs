#![feature(associated_type_defaults)]
extern crate core;
extern crate l4_sys;
extern crate l4_derive;
extern crate l4;
extern crate l4re;

use l4_sys::{l4_utcb, l4_utcb_t};
use l4::{error::Result,
    iface_enumerate, iface_back, derive_ipc_calls, write_msg,
    ipc,
    ipc::MsgTag};
use l4_derive::l4_callable;

iface_enumerate! {
    trait CalcIface {
        const PROTOCOL_ID: i64 = 0x44;
        type OpType = i32;
        fn sub(&mut self, a: u32, b: u32) -> i32;
        fn neg(&mut self, a: u32) -> i32;
    }
    struct Calc;
}

#[l4_callable]
struct CalcServer;

impl CalcIface for CalcServer {
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
    let mut srv_impl = CalcServer::new();
    let mut srv_loop = unsafe {
        ipc::Loop::new_at(
        (*l4re::sys::l4re_env()).main_thread, l4_utcb())
    };
    srv_loop.register(chan, &mut srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}
