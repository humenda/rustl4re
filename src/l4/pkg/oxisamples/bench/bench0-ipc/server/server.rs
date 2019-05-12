extern crate core;
extern crate l4;
extern crate l4re;

use l4::{ipc::MsgTag,
    sys::l4_utcb, sys as l4_sys,
};
use l4re::{OwnedCap, mem::Dataspace};
use l4::sys::util::rdtscp;

const MAX_RUNS: usize = 1000;

// set up shared memory for collection measurements from the server
fn setup_shm<'a>() -> (OwnedCap<Dataspace>, &'a mut [u64]) 
        where OwnedCap<Dataspace>: 'a {
    use l4::sys::*;
    let ds = OwnedCap::<Dataspace>::alloc();
    unsafe {
        let br: *mut l4_buf_regs_t = l4_utcb_br();
        (*br).bdr = 0;
        (*br).br[0] = ds.raw()
                | l4_msg_item_consts_t::L4_RCV_ITEM_SINGLE_CAP as u64
                | l4_msg_item_consts_t::L4_RCV_ITEM_LOCAL_ID as u64;
        let mut label = 0;
        let tag = MsgTag::from(l4_ipc_wait(l4_utcb(), &mut label,
                timeout_never())).result().unwrap();
        if tag.words() != 1 && tag.items() != 1 {
            panic!("Err, received {} words and {} items", tag.words(), tag.items());
        }
        let size = (*l4_utcb_mr()).mr[0] as usize;
        let mut ds_start: *mut libc::c_void = 0 as *mut _;
        // attach received dataspace to virtual memory region and let the
        // measurement functions point to it
        let err = l4re::sys::l4re_rm_attach(
                (&mut ds_start) as *mut *mut _, size as u64,
                l4re::sys::l4re_rm_flags_t::L4RE_RM_SEARCH_ADDR as u64,
                ds.raw(), 0, l4::sys::L4_PAGESHIFT as u8);
        if err != 0 {
            panic!("error while attaching memory: {}", err);
        }
        (ds, core::slice::from_raw_parts_mut(ds_start as *mut _ as *mut u64, size))
    }
}

fn main() {
    println!("Starting server");
    let chan = l4re::env::get_cap::<l4::cap::Untyped>("channel").expect(
            "Received invalid capability for benchmark server.");
    // bind gate
    unsafe {
        match l4_sys::l4_ipc_error(l4_sys::l4_rcv_ep_bind_thread(chan.raw(),
                (*l4re::sys::l4re_env()).main_thread, 0xc01dc0ffee), l4_utcb()) {
            0 => (),
            n => panic!("Error while binding IPC gate: {}", n)
        };
    }

    let (_ds, srv_measurements) = setup_shm(); // needs to be there to be dropped later on
    let mut label = 0;
    unsafe {
        let mut _tag = MsgTag::from(l4_sys::l4_ipc_wait(l4_utcb(),
                &mut label, l4_sys::timeout_never())).result().unwrap();
        let mut _tag = MsgTag::from(l4_sys::l4_ipc_reply_and_wait(l4_utcb(),
                MsgTag::new(0, 0, 0, 0).raw(), &mut label, l4_sys::timeout_never())).result().unwrap();

        for i in 0..(MAX_RUNS) {
            srv_measurements[i] = 0;
        }
        for i in 0..(MAX_RUNS) {
            _tag = MsgTag::from(l4_sys::l4_ipc_reply_and_wait(l4_utcb(),
                    MsgTag::new(0, 0, 0, 0).raw(),
                    &mut label as *mut _, l4_sys::timeout_never())).result().unwrap();
            srv_measurements[i] = rdtscp();
        }
        // final reply needs to be sent to client, didn't figure out how to send reply without wait
        _tag = MsgTag::from(l4_sys::l4_ipc_reply_and_wait(l4_utcb(),
                MsgTag::new(0, 0, 0, 0).raw(),
                &mut label as *mut _, l4_sys::timeout_never())).result().unwrap();
    }
    println!("Done");
}
