

iface! {
    trait Flta {
        const PROTOCOL_ID: i64 = 0x85;
        fn clearance(&mut self, start_alt: f64, start_lat: f64, start_lon: f64,
            pitch: f64, bearing: f64, speed: f64, dur: f64, res: f64)
            -> Option<f64>;
    }
}
