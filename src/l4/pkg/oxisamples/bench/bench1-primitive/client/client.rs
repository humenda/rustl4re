#![feature(associated_type_defaults)]
extern crate core;
extern crate l4re;
extern crate l4;
extern crate l4_derive;

// required for interface initialisation
use l4::cap::Cap;
#[cfg(bench_serialisation)]
use l4::{ipc::MsgTag, error::Error};
use l4_derive::{iface, l4_client};
#[cfg(bench_serialisation)]
use l4re::{OwnedCap, mem::Dataspace};
use core::{arch::x86_64::_rdtsc as rdtsc, iter::Iterator};

include!("../../interface.rs");

#[l4_client(Bencher)]
struct Bench;

// automated drop for shared memory
#[cfg(bench_serialisation)]
struct Shm(*mut u8, l4re::OwnedCap<Dataspace>);
#[cfg(bench_serialisation)]
impl Shm {
    pub fn new(srv: &Cap<Bench>) -> Result<Self, String> {
        use l4::sys::*;
        let size = round_page(l4::MEASURE_RUNS
                * core::mem::size_of::<l4::ServerDispatch>());
        // allocate memory via dataspace manager, attach it
        let (addr, cap) = Self::allocate_bench_mem(size)?;
        unsafe {
            // send cap to server without the interface usage to avoid benchmarking
            let mr = l4_utcb_mr();
            (*mr).mr[0] = size as u64;
            (*mr).mr[1] = l4_msg_item_consts_t::L4_ITEM_MAP as u64;
            (*mr).mr[2] = l4_obj_fpage(cap.raw(), 0,
                    L4_fpage_rights::L4_FPAGE_RWX as u8).raw;
            let _ = MsgTag::from(l4::sys::l4_ipc_send(srv.raw(),
                    l4_utcb(), l4::ipc::MsgTag::new(9876, 1, 1, 0).raw(),
                    l4::sys::timeout_never())).result().unwrap();
        }
        // Allows client-side access to server measurements using a global
        unsafe {
            l4::SERVER_MEASUREMENTS = addr as *mut _;
        }
        Ok(Shm(addr, cap))
    }

    fn allocate_bench_mem(size_in_bytes: usize) -> Result<(*mut u8, OwnedCap<Dataspace>), String> {
        let ds = OwnedCap::<Dataspace>::alloc();
        if ds.is_invalid() {
            return Err("Unable to acquire capability slot for l4re".into());
        }
        // allocate memory
        match unsafe { l4re::sys::l4re_ma_alloc(size_in_bytes, ds.raw(), 0) } {
            0 => (),
            n => return Err(Error::from_ipc(n).to_string()),
        };
        assert!(size_in_bytes != 0);

        // attach to region map
        unsafe {
            let mut virt_addr: *mut core::ffi::c_void =
                core::mem::uninitialized();
            let r = l4re::sys::l4re_rm_attach(&mut virt_addr as *mut *mut _ as _,
                                              size_in_bytes as u64,
                                              l4re::sys::l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR as u64,
                                              ds.raw(), 0, l4::sys::L4_PAGESHIFT as u8);
            if r != 0 { // error, free ds
                drop(ds);
                return Err(format!("Allocation failed with exit code {}", r));
            }
            Ok((virt_addr as *mut u8, ds))
        }
    }
}

#[cfg(bench_serialisation)]
impl core::ops::Drop for Shm {
    fn drop(&mut self) {
        unsafe {
            match l4re::sys::l4re_rm_detach(self.0 as _) {
                0 => (),
                r => println!("Allocation failed with {}", r),
            };
        };

        // unmap dataspace from this task
        unsafe {
            let fpage = l4::sys::l4_obj_fpage(self.1.raw(), 0,
                    l4::sys::L4_cap_fpage_rights::L4_CAP_FPAGE_RWSD as u8);
            let _ = MsgTag::from(l4::sys::l4_task_unmap(
                    l4::sys::L4RE_THIS_TASK_CAP as u64, fpage,
                    l4::sys::l4_unmap_flags_t::L4_FP_DELETE_OBJ as u64)).result().unwrap();
        }
    }
}

// iterate over M (ServerDispatch or ClientCall) with an iterator I, applying a function F
fn format_min_median_max<'a, I, F, M: 'a>(iter: I, description: &str, f: F)
        where F: FnMut(M) -> i64,
              I: std::iter::Iterator<Item = M> {
    let mut sorted = iter.map(f).collect::<Vec<i64>>();
    sorted.sort();
    let median = sorted[l4::MEASURE_RUNS / 2];
    println!("{:<30}{:<10}{:<10}{:<10}",
             description, sorted.iter().min().unwrap(),
             median, sorted.iter().max().unwrap());
}

fn main() {
    let mut server: Cap<Bench> = l4re::env::get_cap("channel").expect(
            "Received invalid capability for benchmarking client.");

    #[cfg(bench_serialisation)]
    let _shm = Shm::new(&server).unwrap();

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
        let _ = server.sub(9, 8).unwrap();
        #[cfg(test_string)]
        let _ = server.str2leet(msg).unwrap();
        let end_counter = unsafe { rdtsc() };
        cycles.push((start_counter, end_counter));
    }
    println!("{:<30}{:<10}{:<10}{:<10}", "Value", "Min", "Median", "Max");
    format_min_median_max(cycles.iter(), "Global",
            |(start, end)| end - start);

    #[cfg(bench_serialisation)]
    { evaluate_microbenchmarks(cycles); }
}

#[cfg(bench_serialisation)]
fn evaluate_microbenchmarks(global: Vec<(i64, i64)>) {
    let clnt_iter = || unsafe { l4::CLIENT_MEASUREMENTS.as_slice().iter() };
    let srv_iter = || unsafe { (*l4::SERVER_MEASUREMENTS).as_slice().iter() };
    let clnt_with_srv = || clnt_iter().zip(srv_iter());
    // time call start to serialisation
    format_min_median_max(clnt_iter().zip(global.iter()),
            "Call start → writing args",
            |(c, (start, _))| c.arg_serialisation_start - start);
    // time of argument serialisation
    format_min_median_max(clnt_iter(), "Writing args to registers", |x: &l4::ClientCall| {
        x.arg_serialisation_end - x.arg_serialisation_start
    });
    // message tag creation
    format_min_median_max(clnt_iter(), "Msg tag creation", |x: &l4::ClientCall| {
        x.ipc_call_start - x.arg_serialisation_end
    });
    format_min_median_max(clnt_with_srv(), "IPC send to server",
        |(c, s)| s.loop_dispatch - c.ipc_call_start);
    format_min_median_max(srv_iter(), "Srv loop → interface dispatch",
        |s| s.iface_dispatch - s.loop_dispatch);
    format_min_median_max(srv_iter(), "Iface disp. → opcode dispatch",
        |s| s.opcode_dispatch - s.iface_dispatch);
    format_min_median_max(srv_iter(), "Reading args + exc user impl",
        |s| s.retval_serialisation_start - s.opcode_dispatch);
    format_min_median_max(srv_iter(), "Write return value",
        |s| s.result_returned - s.retval_serialisation_start);
    format_min_median_max(srv_iter(), "Ret val written → hooks",
        |s| s.hook_start - s.result_returned);
    format_min_median_max(srv_iter(), "Calling server loop hooks",
        |s| s.hook_end - s.hook_start);
    format_min_median_max(clnt_with_srv(), "IPC reply to client",
        |(c, s)| c.return_val_start - s.hook_end);
    // return deserialisation
    format_min_median_max(clnt_iter().zip(global.iter()),
        "Read return value", |(c, (_, e))| e - c.return_val_start);
}
