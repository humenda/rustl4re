#![no_std]
use l4::{
    ipc::{call, sleep, MsgTag},
    sys::{l4_msgtag, l4_timeout_t, l4_utcb},
};

pub fn main() {
    // retrieve IPC gate from Ned (created in the *.cfg script file
    let server = l4re::sys::l4re_env_get_cap("channel").unwrap();
    // test whether valid cap received
    if l4::l4_is_invalid_cap(server) {
        panic!("No IPC Gate found.");
    }

    let mut counter = 1;
    loop {
        // dump value in UTCB
        unsafe {
            (*l4::l4_utcb_mr()).mr[0] = counter;
        }
        println!("value written to register");
        // the message tag contains instructions to the kernel and to the other party what is being
        // transfered: no protocol, 1 message register, no flex page and no flags; flex pages are
        // not relevant here
        let send_tag = MsgTag::new(0, 1, 0, 0);
        let utcb = Utcb::current();
        let tag = call(server, &utcb, send_tag, l4_timeout_t { raw: 0 });
        println!("data sent");
        // check for IPC error, if yes, print out the IPC error code, if not, print the received
        // result.
        match l4::l4_ipc_error(tag, l4_utcb()) {
            0 =>
            // success
            {
                println!("Received: {}\n", (*l4::l4_utcb_mr()).mr[0])
            }
            ipc_error => println!("client: IPC error: {}\n", ipc_error),
        };

        sleep(l4_timeout_t { raw: 200 });
        counter += 1;
    }
}
