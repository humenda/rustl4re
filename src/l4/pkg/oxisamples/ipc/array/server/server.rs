// use l4::l4_server;
//
// include IPC interface definition
#![feature(associated_type_defaults)]
extern crate l4;
extern crate l4_derive;
extern crate l4re;

use l4_derive::l4_server;
use l4::error::Result;

include!("../interface.rs");

#[l4_server]
struct ArraySrv;

impl ArrayHub for ArraySrv {
    fn say(&mut self, msg: String) -> Result<()> {
        println!("Client says. `{}`", msg);
        Ok(())
    }

    fn sum_them_all(&mut self, nums: Vec<i32>) -> Result<i64> {
        println!("Summing up numbers");
        Ok(nums.iter().fold(0i64, |sum, i| sum + *i as i64))
    }
}

fn main() {
    let chan = l4re::sys::l4re_env_get_cap("channel").expect(
            "Received invalid channel capability.");
    let mut srv_impl = ArraySrv::new(chan);
    let mut srv_loop = unsafe {
        l4::ipc::LoopBuilder::new_at((*l4re::sys::l4re_env()).main_thread,
            l4::sys::l4_utcb())
        }.build();
    srv_loop.register(&mut srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}
