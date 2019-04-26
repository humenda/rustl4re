#![feature(associated_type_defaults)]
extern crate l4_sys;
extern crate l4_derive;
extern crate l4;
extern crate l4re;

use l4_derive::l4_client;
use l4::cap::Cap;
use l4re::env;

// include IPC interface definition
include!("../interface.rs");

#[l4_client(ArrayHub)]
struct ArrayClient;

pub fn main() {
    let mut server: Cap<ArrayClient> = env::get_cap("channel")
            .expect("Received invalid capability");
    if server.is_invalid() {
        panic!("No IPC gate found.");
    }

    let text = "This is an example.";
    // send an &str (not requiring allocation)
    println!("Instructing the server to say something.");
    let answer = server.efficient_conversation(text).unwrap();
    println!("It replied: {}",  answer);
    // now do the same using an allocated string
    println!("Instructing it again to say something.");
    server.announce(text.to_string()).unwrap();

    println!("Summing a bunch of numbers in an array");
    let nums: Vec<i32> = (50..80i32).collect();
    println!("Sum of all numbers from the BufArray is {}",
            server.sum_them_all(nums).unwrap());
    println!("done");
}
