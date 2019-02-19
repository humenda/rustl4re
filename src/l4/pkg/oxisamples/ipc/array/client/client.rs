#![feature(associated_type_defaults)]
extern crate l4_sys;
extern crate l4_derive;
extern crate l4;
extern crate l4re;

use l4_derive::l4_client;
use l4::cap::Cap;
use l4re::{env};

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

    let text = String::from("This is an example.");
    // send blah blah using the generated client implementation
    println!("Instruction the server to say something.");
    server.say(text).unwrap();
    println!("Summing a bunch of numbers in an array");
    let nums: Vec<i32> = (50..80i32).collect();
    println!("Sum of all numbers from the BufArray is {}",
            server.sum_them_all(nums).unwrap());
    println!("done");
}
