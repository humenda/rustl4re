//! C type definitions, useful if no libc (crate) support is yet available.
//! If libc is available, definitions will be used from there.
#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

#[cfg(target_arch = "aarch64")]
mod aarch64 {
    pub type c_uchar = u8;
    pub type c_char = i8;
    pub type c_schar = i8;
    pub type c_short = i16;
    pub type c_ushort = u16;
    pub type c_int = i32;
    pub type c_uint = u32;
    pub type c_long = i64;
    pub type c_ulong = u64;
    pub type c_longlong = i64;
    pub type c_ulonglong = u64;
    // we want to avoid the feature hassle with core vs std., so define our own type
    pub type c_void = core::ffi::c_void;
}

#[cfg(target_arch = "aarch64")]
pub use aarch64::*;
#[cfg(target_arch = "x86_64")]
pub use libc::*;
