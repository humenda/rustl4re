extern crate core;
extern crate l4re;
extern crate l4;

use l4::{cap::{Cap, Untyped}, error::Error, ipc::MsgTag};
use l4::sys as l4_sys;
use l4re::{OwnedCap, mem::Dataspace};
use l4::sys::util::rdtscp;

const MAX_RUNS: usize = 1000;
struct Shm(*mut u8, l4re::OwnedCap<Dataspace>);

impl Shm {
    pub fn new(srv: &Cap<Untyped>) -> Result<Self, String> {
        use l4::sys::*;
        let size = round_page(MAX_RUNS * core::mem::size_of::<u64>());
        // allocate memory via dataspace manager, attach it
        let (addr, cap) = Self::allocate_bench_mem(size)?;
        unsafe {
            // send cap to server
            let mr = l4_utcb_mr();
            (*mr).mr[0] = size as u64;
            (*mr).mr[1] = l4_msg_item_consts_t::L4_ITEM_MAP as u64;
            (*mr).mr[2] = l4_obj_fpage(cap.raw(), 0,
                    L4_fpage_rights::L4_FPAGE_RWX as u8).raw;
            // Allows client-side access to server measurements using a global
            let _ = MsgTag::from(l4::sys::l4_ipc_send(srv.raw(),
                    l4_utcb(), l4::ipc::MsgTag::new(9876, 1, 1, 0).raw(),
                    l4::sys::timeout_never())).result().unwrap();
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

    fn as_slice<'a>(&'a self) -> &'a [u64] {
        unsafe {
            core::slice::from_raw_parts(self.0 as *const u64, MAX_RUNS)
        }
    }
}

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

#[inline]
fn call(dst: &Cap<Untyped>, tag: MsgTag) -> l4_sys::l4_msgtag_t {
    unsafe {
        l4_sys::l4_ipc_call(dst.raw(), l4_sys::l4_utcb(), tag.raw(),
                l4_sys::timeout_never())
    }
}

fn main() {
    let server: Cap<Untyped> = l4re::env::get_cap("channel").expect(
            "Received invalid capability for benchmarking client.");

    let shm = Shm::new(&server).unwrap();

    // cold cache call (twice)
    let _ = MsgTag::from(call(&server, MsgTag::new(1, 1, 0, 0))).result().unwrap();
    let _ = MsgTag::from(call(&server, MsgTag::new(1, 1, 0, 0))).result().unwrap();
    let mut client_measurements = [0u64; MAX_RUNS];
    for i in 0..MAX_RUNS {
        let tag = MsgTag::new(1, 1, 0, 0);
        unsafe { (*l4_sys::l4_utcb_mr()).mr[0] = i as u64; }
        client_measurements[i] = rdtscp();
        let _ = MsgTag::from(call(&server, tag)).result().unwrap();
    }
    let srv_stamps = shm.as_slice();
    let mut delta: Vec<u64> = client_measurements[..].iter().enumerate().map(
        |(i, timestamp)| srv_stamps[i] - timestamp).collect();
    delta.sort();
    println!("Min: {}, max: {}", delta[0], delta.last().unwrap());
    println!("Lower quartil: {}, median: {}, upper quartil {}",
            delta[delta.len()/4], delta[delta.len()/2], delta[delta.len()/4*3]);
    println!("-------------------------");
    for (i,timestamp) in client_measurements[..].iter().enumerate() {
        println!("{}", srv_stamps[i] - timestamp);
    }
}
