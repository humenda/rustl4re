use l4_sys::l4_fpage_t;

extern "C" {
    pub fn write_long_float_bool() -> *mut u8;
    pub fn write_bool_long_float() -> *mut u8;
}

mod private {
    extern "C" {
        pub fn write_cxx_snd_fpage(c: *mut u8, fp: ::l4_sys::l4_fpage_t) -> ::libc::c_void;
    }
}
pub unsafe fn write_cxx_snd_fpage(mr: *mut ::l4_sys::l4_msg_regs_t, fp: ::l4_sys::l4_fpage_t) {;
    private::write_cxx_snd_fpage((*mr).mr.as_mut_ptr() as *mut u64 as *mut u8, fp);
}

