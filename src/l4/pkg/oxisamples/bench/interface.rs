iface! {
    trait Bencher {
        const PROTOCOL_ID: i64 = 0xbadc0ffee;
        fn sub(&mut self, a: usize, b: usize) -> i32;
        fn str_ping_pong(&mut self, input: &str) -> &str;
        fn string_ping_pong(&mut self, input: String) -> String;
        fn map_cap(&mut self, ds: l4::cap::Cap<l4re::mem::Dataspace>);
    }
}
