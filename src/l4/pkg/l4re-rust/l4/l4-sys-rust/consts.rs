use c_api::*;

// redefined constants and enums with (wrongly) generated type
#[cfg(target_arch = "x86_64")]
pub const UTCB_GENERIC_DATA_SIZE: usize = 
        L4_utcb_consts_amd64::L4_UTCB_GENERIC_DATA_SIZE as usize;
#[cfg(target_arch = "x86_64")]
pub const UTCB_BUF_REGS_OFFSET: isize = L4_utcb_consts_amd64::L4_UTCB_BUF_REGS_OFFSET
        as isize;

// Constants for message items.
//
// These constants have been redefined as Rust enum, because the constants from
// Bindgen carry constatly the wrong type, the implementor may now choose which
// one to pick. In this library, usize as type is required.
// The names have been stripped of their L4 prefix and the _t suffix, but are
// identical otherwise.
pub const MSGTAG_ERROR: i64 = l4_msgtag_flags::L4_MSGTAG_ERROR as i64;

