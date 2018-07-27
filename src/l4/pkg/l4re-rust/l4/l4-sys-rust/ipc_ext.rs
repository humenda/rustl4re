use c_api::*;
use consts::*;

use ipc_basic::{self, l4_utcb, l4_utcb_mr_u};

#[inline]
pub fn msgtag(label: i64, words: u32,
        items: u32, flags: u32) -> l4_msgtag_t {
    unsafe {
        ipc_basic::l4_msgtag(label, words, items, flags)
    }
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
    ((t.raw >> 6) & 0x3f) as usize
}

#[inline]
pub fn msgtag_label(t: l4_msgtag_t) -> i64 {
    t.raw >> 16
}

#[inline]
pub fn msgtag_words(t: l4_msgtag_t) -> u32 {
    (t.raw & 0x3f) as u32
}

#[inline]
pub fn sndfpage_add_u(snd_fpage: l4_fpage_t, snd_base: u64,
                  tag: &mut l4_msgtag_t, utcb: *mut l4_utcb_t) -> i32 {
    let i = msgtag_words(*tag) as usize + 2 * msgtag_items(*tag);
    if i >= (UTCB_GENERIC_DATA_SIZE - 1) {
        return L4_ENOMEM as i32 * -1;
    }

    unsafe {
        let v = l4_utcb_mr_u(utcb);
        (*v).mr[i] = snd_base
                | L4_ITEM_MAP as u64 | L4_ITEM_CONT as u64;
        (*v).mr[i + 1] = snd_fpage.raw;
    }

    *tag = msgtag(msgtag_label(*tag), msgtag_words(*tag),
                   msgtag_items(*tag) as u32 + 1, msgtag_flags(*tag) as u32);
    0
}

#[inline]
pub unsafe fn sndfpage_add(snd_fpage: l4_fpage_t, snd_base: u64,
        tag: &mut l4_msgtag_t) -> i32 {
    sndfpage_add_u(snd_fpage, snd_base, tag, l4_utcb())
}


#[inline]
pub fn map_control(snd_base: l4_umword_t, cache: u8,
         grant: u32) -> l4_umword_t {
    (snd_base & L4_FPAGE_CONTROL_MASK)
                   | ((cache as l4_umword_t) << 4)
                   | L4_ITEM_MAP as u64
                   | (grant as l4_umword_t)
}

