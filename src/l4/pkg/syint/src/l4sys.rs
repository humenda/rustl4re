use l4_sys::{self, l4_cap_idx_t, helpers as l4helpers,
        l4_cap_consts_t_L4_INVALID_CAP};

const L4_INVALID_CAP: u64 = l4_cap_consts_t_L4_INVALID_CAP as u64;

extern "C" {
    fn l4_is_invalid_cap_w(c: l4_cap_idx_t) -> u32;
}

tests! {
    fn reimpl_l4_is_invalid_cap() {
        unsafe {
            assert_eq!((l4_is_invalid_cap_w(L4_INVALID_CAP) > 0),
                    l4_sys::l4_is_invalid_cap(L4_INVALID_CAP));
        }
    }

    fn rust_and_cstr_equality_works() {
        unsafe {
            assert!(l4helpers::eq_str_cstr("Guten Tag", "Guten Tag\0".as_ptr()));
        }
    }

    fn rust_longer_str_than_cstr_detected() {
        unsafe {
            assert!(!l4helpers::eq_str_cstr("Moin!", "Moin\0".as_ptr()));
            assert!(!l4helpers::eq_str_cstr("Moin dach!", "Moin dach\0".as_ptr()));
        }
    }

    fn rust_longer_cstr_than_ruststr_yields_inequal() {
        unsafe {
            assert!(!l4helpers::eq_str_cstr("Moin", "Moin!\0".as_ptr()));
            assert!(!l4helpers::eq_str_cstr("Moin", "Moin dach vun C!\0".as_ptr()));
        }
    }
}
