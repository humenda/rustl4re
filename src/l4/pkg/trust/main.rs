#[no_mangle]
pub extern "C" fn main() {
    for i in 0..9 {
        println!("Hi no. {}", i);
    }
}
