extern crate core;
extern crate l4_sys;
extern crate l4re_sys;
#[macro_use]
extern crate l4;

// important
use l4::ipc::types::Dispatch;
use l4::ipc::MsgTag;
use l4_sys::l4_utcb;

iface! {
    mod calc;
    trait Calc {
        const PROTOCOL_ID: i64 = 0x44;
        fn sub(&mut self, a: u32, b: u32) -> i32;
        fn neg(&mut self, a: u32) -> i32;
    }
}

struct CalcServer;

impl calc::Calc for CalcServer {
    fn sub(&mut self, a: u32, b: u32) -> i32 {
        let x = a as i32 - b as i32;
        x
    }

    fn neg(&mut self, a: u32) -> i32 {
        a as i32 * -1
    }
}

fn main() {
    println!("Calculation server starting…");
    let chan = l4re_sys::l4re_env_get_cap("calc_server").expect(
            "Received invalid capability for calculation server.");
    unsafe {
        let _ = MsgTag::from(l4_sys::l4_rcv_ep_bind_thread(chan,
                (*l4re_sys::l4re_env()).main_thread, 0xdeadbeef))
            .result().expect("failed to bind gate");
    }
    println!("Waiting for incoming connections");

    let mut label: u64 = unsafe { ::std::mem::uninitialized() };
    let mut tag = unsafe { ::l4::ipc::MsgTag::from(
                l4_sys::l4_ipc_wait(l4_sys::l4_utcb(), &mut label,
                    l4_sys::timeout_never())).result().unwrap()
    };
    let mut srv = Calc::from_impl(chan, CalcServer { });
    loop {
        tag.result().unwrap();
        if label != 0xdeadbeef {
            println!("Hew? Unknown sender {:x}, ignoring that message", label);
        }
        unsafe {
            use l4_sys::l4_utcb_mr;
            let mr = l4_utcb_mr();
            use std::mem::transmute;
            let replytag = srv.dispatch().unwrap().raw();
            println!("returning result and waiting for next job");
            tag = MsgTag::from(l4_sys::l4_ipc_reply_and_wait(l4_utcb(),
                    replytag, &mut label, l4_sys::timeout_never()));
        }
    }
}
