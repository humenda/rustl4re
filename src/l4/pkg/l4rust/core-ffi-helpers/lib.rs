//! Helpers when running on L4Re/Fiasco.OC without STD support.
//!
//! This crate is probably mostly useful for the porting process when no Rust
//! std is available.
//! It also reimplements a few functions that are useful for C FFI interaction,
//! commonly missing when only *core*.
//! For convenience, the functions will use libc or std if std is available.
#![no_std]

pub mod ctypes;

use ctypes::*;

/// Get the length of a C string in bytes.
///
/// # Safety
///
/// The pointer must be valid and the buffer that it points to needs to be
/// terminated by a zero-byte.
#[cfg(not(std))]
pub unsafe fn strlen(c_str: *const u8) -> usize {
    let mut length = 0usize;
    let mut ptr = c_str;
    while *ptr != 0 {
        ptr = ptr.offset(1);
        length += 1
    }
    length
}

#[cfg(std)]
extern "C" {
    pub(crate) fn strlen(c_str: *const u8) -> usize;
}

/// Check for equality of  a Rust string and a C string.
///
/// # Safety
///
/// The C string must be terminated by a zero-byte and the pointer must be
/// valid.
pub unsafe fn eq_str_cstr(name: &str, other: *const c_char) -> bool {
    let mut in_name = other;
    for byte in name.as_bytes() {
        match *in_name {
            0 => return false,
            n if n != (*byte as c_char) => return false,
            _ => (),
        }
        in_name = in_name.offset(1);
    }
    return *in_name == 0; // if 0 reached, strings matched
}
