use l4_derive::iface;

iface! {
    trait ArrayHub {
        const PROTOCOL_ID: i64 = 0xbadf00d;
        fn say(&mut self, msg: &str);
        fn say_conveniently(&mut self, msg: String);
        fn sum_them_all(&mut self, msg: Vec<i32>) -> i64;
    }
}
