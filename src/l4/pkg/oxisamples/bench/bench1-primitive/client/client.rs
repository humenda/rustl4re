#![feature(associated_type_defaults)]
extern crate core;
extern crate l4re;
extern crate l4;
extern crate l4_derive;

// required for interface initialisation
use l4::cap::Cap;
use l4_derive::{iface, l4_client};
use core::{arch::x86_64::_rdtsc as rdtsc, iter::Iterator};

include!("../../interface.rs");

#[l4_client(Bencher)]
struct Bench;

#[cfg(bench_serialisation)]
fn format_min_median_max<F>(description: &str, f: F)
        where F: FnMut(&'static l4::ClientCall) -> i64 {
    let aggregated = unsafe { l4::CLIENT_MEASUREMENTS.as_slice() };
    let aggregated = unsafe { l4::CLIENT_MEASUREMENTS.as_slice() }
            .iter().map(f);
    let mut sorted = aggregated.collect::<Vec<i64>>();
    sorted.sort();
    let median = sorted[l4::MEASURE_RUNS / 2];
    println!("{:<30}{:<10}{:<10}{:<10}",
             description, sorted.iter().min().unwrap(),
             median, sorted.iter().max().unwrap());
}

fn main() {
    let mut client: Cap<Bench> = l4re::env::get_cap("channel").expect(
            "Received invalid capability for benchmarking client.");
    #[cfg(not(test_string))]
    println!("Starting primitive subtraction test");
    #[cfg(test_string)]
    println!("Starting leet speak test");
    let mut cycles = Vec::with_capacity(l4::MEASURE_RUNS);
    #[cfg(test_string)]
    let msg = "Premature optimization is the root of all evil. Yet we should not pass up our opportunities in that critical 3%.";
    for _ in 0..l4::MEASURE_RUNS {
        let start_counter = unsafe { rdtsc() };
        #[cfg(not(test_string))]
        let _ = client.sub(9, 8).unwrap();
        #[cfg(test_string)]
        let _ = client.str2leet(msg).unwrap();
        let end_counter = unsafe { rdtsc() };
        cycles.push(end_counter - start_counter);
    }
    cycles.sort();
    println!("srl → serialisation");
    println!("{:<30}{:<10}{:<10}{:<10}", "Value", "Min", "Median", "Max");
    println!("{:<30}{:<10}{:<10}{:<10}",
             "Global",
             cycles.iter().min().unwrap(),
             cycles[l4::MEASURE_RUNS / 2], cycles.iter().max().unwrap());
    #[cfg(bench_serialisation)]
    { evaluate_microbenchmarks(); }
}

#[cfg(bench_serialisation)]
fn evaluate_microbenchmarks() {
    // time call start to serialisation
    format_min_median_max("call start to arg srl", |x: &l4::ClientCall| {
        x.arg_serialisation_start - x.call_start
    });
    // time of argument serialisation
    format_min_median_max("argument srl", |x: &l4::ClientCall| {
        x.arg_serialisation_end - x.arg_serialisation_start
    });
    // message tag creation
    format_min_median_max("msg tag creation", |x: &l4::ClientCall| {
        x.call_start - x.arg_serialisation_end
    });
    // call length
    format_min_median_max("IPC call", |x: &l4::ClientCall| {
        x.return_val_start - x.call_start
    });
    // return deserialisation
    format_min_median_max("Return value desrl", |x: &l4::ClientCall| {
        x.call_end - x.return_val_start
    });
}
