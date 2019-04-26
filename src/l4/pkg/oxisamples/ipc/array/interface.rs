use l4_derive::iface;

iface! {
    trait ArrayHub {
        const PROTOCOL_ID: i64 = 0xbadf00d;
        fn efficient_conversation(&mut self, msg: &str) -> &str;
        fn announce(&mut self, msg: String);
        fn sum_them_all(&mut self, msg: Vec<i32>) -> i64;
    }
}
