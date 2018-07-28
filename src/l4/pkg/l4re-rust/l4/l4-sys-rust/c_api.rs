#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

//extern "C" {
//    #[no_mangle]
//    pub static l4re_global_env: *const l4re_env_t;
//}
