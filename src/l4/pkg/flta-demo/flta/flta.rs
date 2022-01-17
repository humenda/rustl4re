// This doesn't work with stable Rust, use "extern crate libc" here:
//#![feature(libc)]
extern crate l4_sys;
extern crate l4re;
extern crate l4;
extern crate l4_derive;

use l4_sys::{l4_utcb, l4_mword_t};
use l4_derive::{iface, l4_server};
use l4::{error::Result, threads as l4threads, ipc};
use terrain_database::database::{SimpleTerrainDatabase, TerrainDatabase};
use geo::Point;
use terrain_database::prediction::gen_path;
use uom::si::f64::{Length, Angle, Time, Velocity};
use uom::si::length::foot;
use uom::si::angle::degree;
use uom::si::time::millisecond;
use uom::si::velocity::foot_per_second;
use taws_primitives::Position;

include!("../interface.rs");

#[l4_server(Flta)]
struct FltaServer{
    database: SimpleTerrainDatabase<10000, 10000>,
    pool: rayon::ThreadPool,
}

impl Flta for FltaServer{
    fn clearance(&mut self, start_alt: f64, start_lat: f64, start_lon: f64,
                 pitch: f64, bearing: f64, speed: f64, dur: f64, res: f64)
                 -> Result<Option<f64>>{
        let start = Position{
            alt: Length::new::<foot>(start_alt),
            lat: Angle::new::<degree>(start_lat),
            lon: Angle::new::<degree>(start_lon)
        };
        let pitch = Angle::new::<degree>(pitch);
        let bearing = Angle::new::<degree>(bearing);
        let speed = Velocity::new::<foot_per_second>(speed);
        let dur = Time::new::<millisecond>(dur);
        let res = Length::new::<foot>(res);

        let clearance = self.pool.install(|| {
            let mut i = 0;
            let prediction = gen_path(start.alt, pitch, speed);
            let mut clearance = self.database.clearance(prediction, start.clone(), bearing, dur, res);
            loop {
                let prediction = gen_path(start.alt, pitch, speed);
                clearance = self.database.clearance(prediction, start.clone(), bearing, dur, res);
                i += 1;
                println!("{}", i);
                if i == 100{
                    break;
                }
            }
            clearance
        });

        Ok(clearance.map(|c| c.get::<foot>()))
    }
}

pub fn main() {
    //Initialise
    let database = build_database();

    // get IPC gate capability from Lua script (see ../*.cfg)
    let gate = l4re::sys::l4re_env_get_cap("channel").unwrap();
    // check whether we got something
    if l4_sys::l4_is_invalid_cap(gate) {
        panic!("No IPC Gate found.");
    }

    // Build ThreadPool
    let num_cores = l4threads::num_cores(0, 0, 1).expect("Didn't get number of usable cores").0;
    println!("Num Cores: {}", num_cores);
    let num_cores = 4;
    let mut cores = (0..(num_cores)).collect::<Vec<_>>();
    let pool = rayon::ThreadPoolBuilder::new()
        .spawn_handler(|thread| {
            let mut t = std::thread::spawn(|| thread.run());
            l4threads::sched_cpu_set(&mut t, cores.pop().unwrap(), 2, 0, 0, 1).unwrap();

            Ok(())
        })
        .num_threads(num_cores as usize)
        .build().unwrap();

    let mut srv_impl = FltaServer::new(gate, database, pool);
    let mut srv_loop = unsafe {
        ipc::LoopBuilder::new_at((*l4re::sys::l4re_env()).main_thread,
                                 l4_utcb())
    }.build();
    srv_loop.register(&mut srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}

// 10000 * 10000 * size_of(u64) = 800MB
const DATA: [[u64; 10000]; 10000] = [[0; 10000]; 10000];

fn build_database() -> SimpleTerrainDatabase<10000, 10000> {
    let start = Point::<f64>::new(10.0, 45.0);
    let step = Point::<f64>::new(0.0002778, 0.0002778);

    SimpleTerrainDatabase::new(&DATA, start, step)
}

pub unsafe fn unsafe_main() {

    //// bind IPC gate to main thread with custom label
    //match l4_ipc_error(l4_sys::l4_rcv_ep_bind_thread(gate,
    //        (*l4re::sys::l4re_env()).main_thread, gatelabel), l4_utcb()) {
    //    0 => (),
    //    n => panic!("Error while binding IPC gate: {}", n)
    //};
    //println!("IPC gate received and bound.");

    //println!("waiting for incoming connections");
    //// Wait for requests from any thread. No timeout, i.e. wait forever. Note that the label of the
    //// sender will be placed in `label`.
    //let mut tag = l4_sys::l4_ipc_wait(l4_sys::l4_utcb(), &mut label,
    //                                  l4_sys::l4_timeout_t { raw: 0 });

    //loop {
    //    // Check if we had any IPC failure, if yes, print the error code and
    //    // just wait again.
    //    let ipc_error = l4_sys::l4_ipc_error(tag, l4_sys::l4_utcb());
    //    if ipc_error != 0 {
    //        println!("server: IPC error: {}\n", ipc_error);
    //        tag = l4_sys::l4_ipc_wait(l4_sys::l4_utcb(), &mut label,
    //                                  l4_sys::l4_timeout_t { raw: 0 });
    //        continue;
    //    }

    //    // IPC was ok, now get value from message register 0 of UTCB and store the square of it
    //    // back to it
    //    let query: [u32; 16] = (*l4_sys::l4_utcb_mr()).mr.iter()
    //        .take(16).cloned().map(|i| i as u32).collect::<Vec<_>>().try_into().unwrap();
    //    let query = ClearanceQuery::from_u32(query);
    //
    //    let dur = now.elapsed();
    //    println!("Clearance calculation took: {:?}ns", dur.as_nanos());
    //    let clearance_u32: [u32; 2] = ClearanceQuery::length_to_u32(clearance);
    //    (*l4_sys::l4_utcb_mr()).mr[0] = clearance_u32[0] as l4_umword_t;
    //    (*l4_sys::l4_utcb_mr()).mr[1] = clearance_u32[1] as l4_umword_t;
    //    //println!("new value: {}", (*l4::l4_utcb_mr()).mr[0]);

    //    // send reply and wait again for new messages; the message tag specifies to send one
    //    // machine word with no protocol, no flex pages and no flags
    //    tag = l4_sys::l4_ipc_reply_and_wait(l4_sys::l4_utcb(), l4_msgtag(0, 2, 0, 0),
    //                          &mut label, l4_sys::l4_timeout_t { raw: 0 });
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