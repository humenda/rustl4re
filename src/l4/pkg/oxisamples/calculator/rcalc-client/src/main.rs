// required for interface initialisation
use l4::cap::IfaceInit;
use l4_derive::{iface, l4_client};

// include shared interface definition (located relative to the directory of
// this file)
include!("../../interface.rs");

#[l4_client(Calculator)]
struct Calc;

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
