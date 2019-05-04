extern crate l4;
extern crate l4_derive;
extern crate l4re;

use l4_derive::l4_server;
use l4::{error::Result,
    ipc::server::TypedBuffer,
};

include!("../interface.rs");

#[l4_server(ArrayHub, buffer=200)]
struct ArraySrv;

impl ArrayHub for ArraySrv {
    // client said hi, let's reply
    fn greeting(&mut self, msg: &str) -> Result<&str> {
        // &str points to message registers, every syscall (including printing to the console) will
        // destroy this data, copy to stack, into cache. We could however skip
        // this step, since we are printing it anyway, which is a syscall and we
        // do not need the data afterwards.
        let opening = self.copy_in(msg);
        println!("Client says: `{}`", opening?);
        // answer back
        Ok(self.copy_in("Enchantez, hello!")?)
    }

    // uses heap-allocated String: convenient, but expensive
    fn announce(&mut self, msg: String) -> Result<()> {
        // in contrast to say(), we get an allocated string from the framework, so we're good to go
        println!("Client says: `{}`", msg);
        Ok(())
    }

    fn sum_them_all(&mut self, nums: Vec<i32>) -> Result<i64> {
        println!("Summing up numbers");
        Ok(nums.iter().fold(0i64, |sum, i| sum + *i as i64))
    }

}

fn main() {
    let chan = l4re::sys::l4re_env_get_cap("channel").expect(
            "Received invalid channel capability.");
    let mut srv_impl = ArraySrv::new(chan);
    let mut srv_loop = unsafe {
        l4::ipc::LoopBuilder::new_at((*l4re::sys::l4re_env()).main_thread,
            l4::sys::l4_utcb())
        }.build();
    srv_loop.register(&mut srv_impl).expect("Failed to register server in loop");
    println!("Waiting for incoming connections");
    srv_loop.start();
}
