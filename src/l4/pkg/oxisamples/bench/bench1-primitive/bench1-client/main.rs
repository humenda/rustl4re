#![feature(associated_type_defaults)]
extern crate core;
extern crate l4_sys;
extern crate l4re;
extern crate l4;
extern crate l4_derive;

// required for interface initialisation
use l4::cap::Cap;
use l4_derive::{iface, l4_client};
use core::arch::x86_64::_rdtsc as rdtsc;

include!("../../interface.rs");

#[l4_client(Bencher)]
struct Bench;

fn main() {
    let mut client: Cap<Bench> = l4re::env::get_cap("channel").expect(
            "Received invalid capability for benchmarking client.");
    #[cfg(not(test_string))]
    println!("Starting primitive subtraction test");
    #[cfg(test_string)]
    println!("Starting string ping pong test");
    let mut cycles = Vec::with_capacity(100000);
    for _ in 0..100000 {
        // value is moved, hence reallocate it
        #[cfg(test_string)]
        let freshly_allocated = String::from("Premature optimization is the root of all evil. Yet we should not pass up our opportunities in that critical 3%.");
        let start_counter = unsafe { rdtsc() };
        #[cfg(not(test_string))]
        let _ = client.sub(9, 8).unwrap();
        #[cfg(test_string)]
        let _ = client.strpingpong(freshly_allocated).unwrap();
        let end_counter = unsafe { rdtsc() };
        cycles.push(end_counter - start_counter);
    }
    cycles.sort();
    println!("min: {}", cycles.iter().min().unwrap());
    println!("max: {}", cycles.iter().max().unwrap());
    println!("avg: {}", cycles.iter().sum::<i64>() / cycles.len() as i64);
    println!("median: {}", cycles[500]);
}
