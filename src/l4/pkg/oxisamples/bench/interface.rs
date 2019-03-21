iface! {
    trait Bencher {
        const PROTOCOL_ID: i64 = 0xbadc0fee;
        fn sub(&mut self, a: u32, b: u32) -> i32;
        fn strpingpong(&mut self, input: String) -> String;
    }
}
