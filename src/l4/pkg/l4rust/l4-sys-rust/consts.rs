//! Type definitions and constants.
use crate::c_api::*;

/// An alias for the corresponding platform enum
#[cfg(target_arch = "x86_64")]
pub use crate::c_api::L4_utcb_consts_amd64 as UtcbConsts;
#[cfg(target_arch = "aarch64")]
pub use crate::c_api::L4_utcb_consts_arm64 as UtcbConsts;
#[cfg(target_arch = "x86")]
pub use L4_utcb_consts_x86 as UtcbConsts;

// redefined constants and enums with (wrongly) generated type
pub const UTCB_GENERIC_DATA_SIZE: usize = UtcbConsts::L4_UTCB_GENERIC_DATA_SIZE as usize;
pub const UTCB_BUF_REGS_OFFSET: isize = UtcbConsts::L4_UTCB_BUF_REGS_OFFSET as isize;

// Constants for message items.
//
// These constants have been redefined as Rust enum, because the constants from
// Bindgen carry constatly the wrong type, the implementor may now choose which
// one to pick. In this library, usize as type is required.
// The names have been stripped of their L4 prefix and the _t suffix, but are
// identical otherwise.
pub const MSGTAG_ERROR: i64 = l4_msgtag_flags::L4_MSGTAG_ERROR as i64;

/// 0 send timeout
pub const L4_IPC_SEND_TIMEOUT_0: l4_timeout_t = l4_timeout_t { raw: 0x04000000 };
