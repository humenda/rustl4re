use c_api::*;

pub const FPAGE_RWX: u8 = L4_FPAGE_RWX as u8;

// redefined constants and enums with (wrongly) generated type
#[cfg(target_arch = "x86_64")]
pub const UTCB_GENERIC_DATA_SIZE: usize = L4_UTCB_GENERIC_DATA_SIZE as usize;
#[cfg(target_arch = "x86_64")]
pub const UTCB_BUF_REGS_OFFSET: isize = L4_UTCB_BUF_REGS_OFFSET as isize;

// Constants for message items.
//
// These constants have been redefined as Rust enum, because the constants from
// Bindgen carry constatly the wrong type, the implementor may now choose which
// one to pick. In this library, usize as type is required.
// The names have been stripped of their L4 prefix and the _t suffix, but are
// identical otherwise.
/// Identify a message item as **map** item.
pub const MSG_ITEM_CONSTS_ITEM_MAP: u64 = 8;
/// Denote that the following item shall be put into the same receive item as this one.
pub const MSG_ITEM_CONSTS_ITEM_CONT: u64 = 1;
///< Flag as usual **map** operation.
// receive
/// Mark the receive buffer to be a small receive item that describes a buffer for a single
/// capability.
pub const MSGTAG_ERROR: i64 = L4_MSGTAG_ERROR as i64;


