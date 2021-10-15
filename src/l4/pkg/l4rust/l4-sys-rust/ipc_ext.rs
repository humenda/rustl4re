use crate::c_api::{l4_msgtag_flags::*, *};
use crate::ipc_basic::{self, l4_utcb};

/// Provide a convenient mechanism to access the message registers
macro_rules! mr {
    ($v:ident[$w:expr] = $stuff:expr) => {
        (*$v).mr.as_mut()[$w as usize] = $stuff as u64;
    };
}

#[inline]
pub fn msgtag(label: i64, words: u32, items: u32, flags: u32) -> l4_msgtag_t {
    ipc_basic::l4_msgtag(label, words, items, flags)
}

#[inline]
pub fn msgtag_flags(t: l4_msgtag_t) -> u32 {
    (t.raw & 0xf000) as u32
}

#[inline]
pub fn msgtag_has_error(t: l4_msgtag_t) -> bool {
    (t.raw & L4_MSGTAG_ERROR as i64) != 0
}

#[inline]
pub fn msgtag_items(t: l4_msgtag_t) -> usize {
    ipc_basic::l4_msgtag_items(t)
}

#[inline]
pub fn msgtag_label(t: l4_msgtag_t) -> i64 {
    ipc_basic::l4_msgtag_label(t)
}

#[inline]
pub fn msgtag_words(t: l4_msgtag_t) -> u32 {
    ipc_basic::l4_msgtag_words(t)
}

/// Add a flex-page for sending
///
/// This function adds a flex page to the first message register of the UTCB.
/// Additionally, the given message tag is modified by increasing the word count.
/// A return code of 0 denotes success, errors are negative.
#[inline]
pub unsafe fn sndfpage_add_u(
    snd_fpage: l4_fpage_t,
    snd_base: u64,
    tag: &mut l4_msgtag_t,
    utcb: *mut l4_utcb_t,
) -> i32 {
    ipc_basic::l4_sndfpage_add_u(snd_fpage, snd_base, tag, utcb)
}

// See `sndfpage_add_u`
#[inline]
pub fn sndfpage_add(snd_fpage: l4_fpage_t, snd_base: u64, tag: &mut l4_msgtag_t) -> i32 {
    unsafe {
        // the only unsafe bit here is passing the  UTCB pointer, but l4_utcb() is gauranteed
        // to work, so the whole operation is safe
        sndfpage_add_u(snd_fpage, snd_base, tag, l4_utcb())
    }
}

#[inline]
pub fn map_control(snd_base: l4_umword_t, cache: u8, grant: u32) -> l4_umword_t {
    ipc_basic::l4_map_control(snd_base, cache, grant)
}
