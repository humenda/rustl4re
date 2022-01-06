// This doesn't work with stable Rust, use "extern crate libc" here:
//#![feature(libc)]
extern crate l4_sys as l4;
extern crate l4re;

#[path="../query.rs"]
mod query;

use libc::c_ulong;
use l4::{l4_ipc_error, l4_msgtag, l4_utcb, l4_umword_t};
use std::convert::TryInto;
use std::time;
use terrain_database::database::{SimpleTerrainDatabase, TerrainDatabase};
use geo::Point;
use query::ClearanceQuery;
use terrain_database::prediction::gen_path;
use uom::si::f64::Length;
use uom::si::length::foot;

pub fn main() {
    unsafe { // avoid deep nesting of unsafe blocks, just make the whole program unsafe
        unsafe_main();
    }
}

// 3000 * 3000 * size_of(u64) = 72MB
const DATA: [[u64; 3000]; 3000] = [[0; 3000]; 3000];

fn build_database() -> SimpleTerrainDatabase<3000, 3000> {
    let start = Point::<f64>::new(10.0, 45.0);
    let step = Point::<f64>::new(0.0002778, 0.0002778);

    SimpleTerrainDatabase::new(&DATA, start, step)
}

pub unsafe fn unsafe_main() {
    //Initialise
    let database = build_database();

    // IPC gates can receive from multiple senders, hence messages need an identification label for
    // clients. Here's one:
    let gatelabel = 0b11111100 as c_ulong;
    // place to put label in when a message is received
    let mut label = std::mem::uninitialized();


    // get IPC gate capability from Lua script (see ../*.cfg)
    let gate = l4re::sys::l4re_env_get_cap("channel").unwrap();
    // check whether we got something
    if l4::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }

    // bind IPC gate to main thread with custom label
    match l4_ipc_error(l4::l4_rcv_ep_bind_thread(gate,
            (*l4re::sys::l4re_env()).main_thread, gatelabel), l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");

    println!("waiting for incoming connections");
    // Wait for requests from any thread. No timeout, i.e. wait forever. Note that the label of the
    // sender will be placed in `label`.
    let mut tag = l4::l4_ipc_wait(l4::l4_utcb(), &mut label,
            l4::l4_timeout_t { raw: 0 });

    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        let ipc_error = l4::l4_ipc_error(tag, l4::l4_utcb());
        if ipc_error != 0 {
            println!("server: IPC error: {}\n", ipc_error);
            tag = l4::l4_ipc_wait(l4::l4_utcb(), &mut label, 
                    l4::l4_timeout_t { raw: 0 });
            continue;
        }

        // IPC was ok, now get value from message register 0 of UTCB and store the square of it
        // back to it
        let query: [u32; 16] = (*l4::l4_utcb_mr()).mr.iter()
            .take(16).cloned().map(|i| i as u32).collect::<Vec<_>>().try_into().unwrap();
        let query = ClearanceQuery::from_u32(query);
        let prediction = gen_path(query.start.alt, query.pitch, query.speed);
        let now = time::Instant::now();
        let clearance = database.clearance(prediction, query.start, query.bearing,
                                           query.dur, query.res).unwrap();
        let dur = now.elapsed();
        println!("Clearance calculation took: {:?}ns", dur.as_nanos());
        let clearance_u32: [u32; 2] = ClearanceQuery::length_to_u32(clearance);
        (*l4::l4_utcb_mr()).mr[0] = clearance_u32[0] as l4_umword_t;
        (*l4::l4_utcb_mr()).mr[1] = clearance_u32[1] as l4_umword_t;
        //println!("new value: {}", (*l4::l4_utcb_mr()).mr[0]);

        // send reply and wait again for new messages; the message tag specifies to send one
        // machine word with no protocol, no flex pages and no flags
        tag = l4::l4_ipc_reply_and_wait(l4::l4_utcb(), l4_msgtag(0, 2, 0, 0),
                              &mut label, l4::l4_timeout_t { raw: 0 });
    }
}

//For some reason the linker finds unresolved references to _Unwind_GetIP
//Because of this we create a dummy function
#[no_mangle]
#[cfg(target_arch = "arm")]
pub extern "C" fn _Unwind_GetIP(_: libc::c_int) {
    panic!("_Unwind_GetIP was called")
}
/*
Alternatively, add this function to the backtrace.c
(src/l4/pkg/l4re-core/l4util/lib/src/ARCH-arm/backtrace.c)

Function:
void _Unwind_GetIP(context){
    while (1);
}
*/