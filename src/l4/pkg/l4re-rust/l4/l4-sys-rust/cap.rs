use std::os::raw::{c_uchar, c_uint};
use c_api::*;

////////////////////////////////////////////////////////////////////////////////
// redefined constants

pub const FPAGE_RWX: u8 = L4_FPAGE_RWX as u8;

////////////////////////////////////////////////////////////////////////////////
// inline functions from l4/sys/env.h:



#[inline]
pub fn l4_is_invalid_cap(cap: l4_cap_idx_t) -> bool {
    (cap & L4_INVALID_CAP_BIT) > 0
}

#[inline]
pub unsafe fn l4_obj_fpage(obj: l4_cap_idx_t, order: c_uint, rights: c_uchar)
        -> l4_fpage_t {
    l4_obj_fpage_w(obj, order, rights)
}

////////////////////////////////////////////////////////////////////////////////
// complete re-implementations

/// Create the first word for a map item for the memory space.
///
/// Returns the value to be used as first word in a map item for memory.
///
/// * n`spot`   Hot spot address, used to determine what is actually mapped when send and receive
///     flex page have differing sizes.
/// * `cache`  Cacheability hints for memory flex pages. See `l4_fpage_cacheability_opt_t`
/// * `grant`  Indicates if it is a map or a grant item.
#[inline]
pub fn l4_map_control(snd_base: l4_umword_t, cache: u8, grant: u64)
        -> l4_umword_t {
    (snd_base & L4_FPAGE_CONTROL_MASK)
        | ((cache as l4_umword_t) << 4)
        | (L4_ITEM_MAP as u64)
        | grant
}

/// Create the first word for a map item for the object space.
///
/// Returns The value to be used as first word in a map item for kernel objects
/// or IO-ports.
/// *   `spot`:   Hot spot address, used to determine what is actually mapped
///     when send and receive flex pages have different size.
/// *   `grant`:  Indicates if it is a map item or a grant item.
pub fn l4_map_obj_control(snd_base: l4_umword_t, grant: u64) -> l4_umword_t {
    l4_map_control(snd_base, 0, grant)
}

