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
