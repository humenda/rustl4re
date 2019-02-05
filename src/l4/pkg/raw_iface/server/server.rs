extern crate l4;
extern crate l4re;

use l4::sys;

fn main() {
    unsafe {
    let chan = l4re::sys::l4re_env_get_cap("channel").expect(
            "Received invalid capability for calculation server.");

    let mut label = std::mem::uninitialized();
    match sys::l4_ipc_error(sys::l4_rcv_ep_bind_thread(chan,
            (*l4re::sys::l4re_env()).main_thread, 0xdeadbeef), sys::l4_utcb()) {
        0 => (),
        n => panic!("Error while binding IPC gate: {}", n)
    };
    println!("IPC gate received and bound.");

    println!("waiting for incoming connections");
    // Wait for requests from any thread. No timeout, i.e. wait forever. Note that the label of the
    // sender will be placed in `label`.
    let mut tag = sys::l4_ipc_wait(sys::l4_utcb(), &mut label,
            sys::l4_timeout_t { raw: 0 });
    loop {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        let ipc_error = sys::l4_ipc_error(tag, sys::l4_utcb());
        if ipc_error != 0 {
            println!("server: IPC error: {}\n", ipc_error);
            tag = sys::l4_ipc_wait(sys::l4_utcb(), &mut label, 
                    sys::l4_timeout_t { raw: 0 });
            continue;
        }

        {
            let tag = l4::ipc::MsgTag::from(tag);
            println!("Received: protocol={:x}, words: {:x}, items: {:x}", tag.label(), tag.words(), tag.items());
        }
        // IPC was ok, now get value from message register 0 of UTCB and store the square of it
        // back to it
        println!("op code (as i32): {:x}", (*core::mem::transmute::<*mut u64, *mut i32>(
                    (*sys::l4_utcb_mr()).mr.as_mut_ptr())));
        println!("first word: {:x}", (*sys::l4_utcb_mr()).mr[1] as u64);
        println!("second word: {:x}", (*sys::l4_utcb_mr()).mr[2] as u64);
        println!("third word: {:x}", (*sys::l4_utcb_mr()).mr[3] as u64);
        let ms = std::time::Duration::from_millis(1000);
        tag = sys::l4_ipc_reply_and_wait(sys::l4_utcb(), sys::l4_msgtag(0, 0, 0, 0),
                              &mut label, sys::l4_timeout_t { raw: 0 });

        std::thread::sleep(ms);
    }
    }
}
