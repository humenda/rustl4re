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

