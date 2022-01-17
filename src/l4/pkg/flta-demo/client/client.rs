extern crate l4_sys;
extern crate l4re;
extern crate l4;
extern crate l4_derive;

use l4_sys::l4_mword_t;
use std::{thread, time};
use uom::si::f64::{Length, Angle, Time, Velocity};
use uom::si::length::foot;
use uom::si::angle::degree;
use uom::si::time::millisecond;
use taws_primitives::Position;
use uom::si::velocity::{foot_per_second, knot};
use uom::si::length::meter;
use uom::si::time::second;
use l4_derive::{iface, l4_client};
use l4::cap::IfaceInit;

include!("../interface.rs");

#[l4_client(Flta)]
struct FltaServer;

pub fn main() {
     let start = Position {
        lon: Angle::new::<degree>(10.0),
        lat: Angle::new::<degree>(45.0),
        //lon: Angle::new::<degree>(10.2778)
        //lat: Angle::new::<degree>(45.2778)
        alt: Length::new::<foot>(1000.0)
    };
    let pitch = Angle::new::<degree>(-10.0);
    let bearing = Angle::new::<degree>(45.0);
    let speed = Velocity::new::<knot>(100.0);
    let dur = Time::new::<second>(600.0);
    let res = Length::new::<meter>(1.0);

    // retrieve IPC gate from Ned (created in the *.cfg script file
    let flta = l4re::sys::l4re_env_get_cap("channel").unwrap();
    // test whether valid cap received
    if l4_sys::l4_is_invalid_cap(flta) {
        panic!("No IPC Gate found.");
    }
    println!("Trying to contact server");
    let mut client = FltaServer::new(flta);

    let mut counter = 1;
    let now = time::Instant::now();
    loop {
        let result = client.clearance(start.alt.get::<foot>(),
            start.lat.get::<degree>(), start.lon.get::<degree>(),
            pitch.get::<degree>(), bearing.get::<degree>(),
            speed.get::<foot_per_second>(), dur.get::<millisecond>(),
            res.get::<foot>());

        //match result{
        //    Err(e) => println!("{:?}", e),
        //    Ok(c) =>
        //        println!("Received: {}\n", c.map(|c| c.to_string() + " ft").unwrap_or_else(|| String::from("None"))),
        //}

        //thread::sleep(time::Duration::from_millis(1000));
        if counter == 1{
            println!("100 calls took: {:?}", now.elapsed());
            return;
        }
        counter += 1;
    }

    //
    //loop {
    //    // dump value in UTCB
    //    unsafe {
    //        for (i, u) in query.to_u32().iter().enumerate(){
    //            (*l4_sys::l4_utcb_mr()).mr[i] = *u as l4_umword_t;
    //        }
    //    }
    //    println!("value written to register");
    //    // the message tag contains instructions to the kernel and to the other party what is being
    //    // transfered: no protocol, 16 message register, no flex page and no flags; flex pages are
    //    // not relevant here
    //    unsafe {
    //        let now = time::Instant::now();
    //        let send_tag = l4_msgtag(0, 16, 0, 0);
    //        let tag = l4_sys::l4_ipc_call(flta, l4_utcb(), send_tag,
    //                                      l4_sys::l4_timeout_t { raw: 0 });
    //        println!("data sent");
    //        // check for IPC error, if yes, print out the IPC error code, if not, print the received
    //        // result.
    //        match l4_sys::l4_ipc_error(tag, l4_utcb()) {
    //            0 => { // success
    //                let clearance = ClearanceQuery::u32_to_length((*l4_sys::l4_utcb_mr()).mr[0..2].iter().cloned().map(|i| i as u32).collect::<Vec<_>>().try_into().unwrap());
    //                //let clearance = (*l4::l4_utcb_mr()).mr[0..2].to_vec().iter().cloned().map(|i| (i as u32).to_be_bytes());
    //                println!("Received: {}\n", clearance.into_format_args(foot, Abbreviation))
    //            }
    //            ipc_error => println!("client: IPC error: {}\n",  ipc_error),
    //        };
    //    }

    //    thread::sleep(time::Duration::from_millis(1000));
    //    counter += 1;
    //}
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
