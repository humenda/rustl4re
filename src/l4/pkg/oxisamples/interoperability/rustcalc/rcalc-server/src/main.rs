extern crate core;
extern crate l4_sys;
extern crate l4re_sys;
#[macro_use]
extern crate l4;

use l4_sys::l4_utcb;
use l4::ipc;

iface! {
    mod calc;
    trait Calc {
        const PROTOCOL_ID: i64 = 0x44;
        fn sub(&mut self, a: u32, b: u32) -> i32;
        fn neg(&mut self, a: u32) -> i32;
    }
}

struct CalcServer;

impl calc::Calc for CalcServer {
    fn sub(&mut self, a: u32, b: u32) -> i32 {
        let x = a as i32 - b as i32;
        x
    }

    fn neg(&mut self, a: u32) -> i32 {
        a as i32 * -1
    }
}

fn main() {
    println!("Calculation server starting…");
    let chan = l4re_sys::l4re_env_get_cap("calc_server").expect(
            "Received invalid capability for calculation server.");
    let srv_impl = Calc::from_impl(chan, CalcServer { });
    let mut srv_loop = unsafe {
        ipc::Loop::new_at(
        (*l4re_sys::l4re_env()).main_thread, l4_utcb())
    };
    srv_loop.register(chan, srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}
