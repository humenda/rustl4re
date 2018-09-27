#[macro_use]
extern crate l4;

iface! {
    mod calc;
    
    struct Calc {
        fn sub(a: u32, b: u32) -> i32;
        fn neg(a: u32) -> i32;
    }
}

fn main() {
    let client = Calc::client(0);
    println!("Hello, world!");
}
