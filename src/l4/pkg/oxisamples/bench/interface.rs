iface! {
    trait Bencher {
        const PROTOCOL_ID: i64 = 0xbadc0ffee;
        fn sub(&mut self, a: u32, b: u32) -> i32;
        fn str_ping_pong(&mut self, input: &str) -> &str;
        fn string_ping_pong(&mut self, input: String) -> String;
        fn check_dataspace(&mut self, size: u64, ds: l4::cap::Cap<l4re::mem::Dataspace>);
    }
}
