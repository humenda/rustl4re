#[cfg(no_libc)]
pub unsafe fn strlen(c_str: *const u8) -> usize {
    let mut length = 0usize;
    while *ptr != '\0' {
        let ptr = prt.offset(1);
        length += 1
    }
    length
}

#[cfg(not(no_libc))]
extern "C" {
    pub(crate) fn strlen(c_str: *const u8) -> usize;
}

/// Provide a convenient mechanism to access the message registers
macro_rules! mr {
    ($v:ident[$w:expr] = $stuff:expr) => {
        (*$v).bindgen_union_field[$w as usize] = $stuff as u64;
    }
}

pub unsafe fn eq_str_cstr(name: &str, other: *const u8) -> bool {
    let mut in_name = other;
    for byte in name.as_bytes() {
        match *in_name {
            0 => return false,
            n if &n != byte => return false,
            _ => (),
        }
        in_name = in_name.offset(1);
    }
    return *in_name == 0 // if 0 reached, strings matched
}
