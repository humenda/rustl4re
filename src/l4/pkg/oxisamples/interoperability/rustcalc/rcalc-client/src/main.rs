#![feature(associated_type_defaults)]
extern crate core;
extern crate l4_sys;
extern crate l4re;
#[macro_use]
extern crate l4;

// required for interface initialisation
use l4::cap::IfaceInit;

iface_enumerate! {
    trait CalcSpec {
        const PROTOCOL_ID: i64 = 0x44;
        type OpType = i32;
        fn sub(&mut self, a: u32, b: u32) -> i32;
        fn neg(&mut self, a: u32) -> i32;
    }
    struct Calc;
}

fn main() {
    println!("Calculation client starting…");
    let chan = l4re::sys::l4re_env_get_cap("calc_server").expect(
            "Received invalid capability for calculation server.");
    println!("Trying to contact server");
    let mut client = Calc::new(chan);
    println!("What is 9 - 8?");
    let res = client.sub(9, 8).unwrap();
    println!("It is: {}", res);
    println!("\n\nWhat is 9898 - 7777?");
    let res = client.sub(9898, 7777).unwrap();
    println!("It is: {}", res);
    println!("What is neg(9)");
    let res = client.neg(9).unwrap();
    println!("It is: {}", res);
}
