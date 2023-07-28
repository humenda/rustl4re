// ToDo: proc macros didn't support doc comments at some point, make them support doc strings and
// reformat the two interfaces
use l4::{
    cap::Cap,
    utcb::SndFlexPage as FlexPage,
    sys::{l4_addr_t, l4_size_t},
};
use l4_derive::{iface, l4_client};
use super::sys::L4ReProtocols::L4RE_PROTO_DATASPACE;
use l4::Error;
use l4::Result;
use l4::cap::CapIdx;
use l4::cap::Interface;
use l4::cap::Cap;
use l4::task::THIS_TASK;
use l4::ipc::MsgTag;
use bitflags::bitflags;
use crate::factory::DmaSpace;
use crate::sys::l4re_ma_alloc;
use crate::sys::l4re_ds_map_flags::*;
use crate::sys::l4re_ma_flags::*;
use crate::sys::l4re_rm_flags_values::*;
use crate::sys::l4re_rm_attach;
use crate::sys::l4re_rm_detach;
use l4_sys::l4re_util_cap_alloc;
use l4_sys::l4_error_code_t::L4_EOK;
use l4_sys::L4_PAGESHIFT;
use l4_sys::l4_task_map;
use l4_sys::l4_msg_item_consts_t::*;
use l4_sys::l4_map_obj_control;
use l4_sys::L4_fpage_rights::*;
use l4_sys::L4_LOG2_PAGESIZE;
use l4_sys::l4_fpage_w;

use core::ffi::c_void;
use core::marker::PhantomData;

use crate::OwnedCap;

// TODO: This file shows that the OwnedCap abstraction doesn't compose well, fix it
// TODO: Consider using super pages for performance?

#[derive(Debug)]
pub struct Dataspace {
    cap : CapIdx,
    size: usize
}

pub struct AttachedDataspace {
    cap: OwnedCap<Dataspace>,
    ptr: *mut u8

}

pub struct OffsetAttachedDataspace<'a> {
    ptr: *mut u8,
    _space : PhantomData<&'a ()>
}

pub trait VolatileMemoryInterface {
    fn ptr(&self) -> *mut u8;

    fn write8(&mut self, offset: usize, val: u8) {
        unsafe { core::ptr::write_volatile(self.ptr().add(offset), val) }
    }

    fn write16(&mut self, offset: usize, val: u16) {
        unsafe { core::ptr::write_volatile(self.ptr().add(offset) as *mut u16, val) }
    }

    fn write32(&mut self, offset: usize, val: u32) {
        unsafe { core::ptr::write_volatile(self.ptr().add(offset) as *mut u32, val) }
    }

    fn write64(&mut self, offset: usize, val: u64) {
        unsafe { core::ptr::write_volatile(self.ptr().add(offset) as *mut u64, val) }
    }

    fn read8(&self, offset: usize) -> u8 {
        unsafe { core::ptr::read_volatile(self.ptr().add(offset)) }
    }

    fn read16(&self, offset: usize) -> u16 {
        unsafe { core::ptr::read_volatile(self.ptr().add(offset) as *mut u16) }
    }

    fn read32(&self, offset: usize) -> u32 {
        unsafe { core::ptr::read_volatile(self.ptr().add(offset) as *mut u32) }
    }

    fn read64(&self, offset: usize) -> u64 {
        unsafe { core::ptr::read_volatile(self.ptr().add(offset) as *mut u64) }
    }
}

bitflags! {
    pub struct MaFlags : u64 {
        const CONTINUOUS = L4RE_MA_CONTINUOUS as u64;
        const PINNED = L4RE_MA_PINNED as u64;
        const SUPER_PAGES = L4RE_MA_SUPER_PAGES as u64;
    }
}

bitflags! {
    pub struct DsMapFlags : u64 {
        const R = L4RE_DS_F_R as u64;
        const W = L4RE_DS_F_W as u64;
        const X = L4RE_DS_F_X as u64;
        const RW = L4RE_DS_F_RW as u64;
        const RX = L4RE_DS_F_RX as u64;
        const RWX = L4RE_DS_F_RWX as u64;
        const RIGHTS_MASK = L4RE_DS_F_RIGHTS_MASK as u64;
        const NORMAL = L4RE_DS_F_NORMAL as u64;
        const BUFFERABLE = L4RE_DS_F_BUFFERABLE as u64;
        const UNCACHEABLE = L4RE_DS_F_UNCACHEABLE as u64;
        const CACHING_MASK = L4RE_DS_F_CACHING_MASK as u64;
    }
}

bitflags! {
    pub struct DsAttachFlags : u64 {
        const SEARCH_ADDR =  L4RE_RM_F_SEARCH_ADDR as u64;
        const IN_AREA = L4RE_RM_F_IN_AREA as u64;
        const EAGER_MAP = L4RE_RM_F_EAGER_MAP as u64;
    }
}

impl Interface for Dataspace {
    #[inline]
    fn raw(&self) -> CapIdx {
        self.cap
    }
}

impl Drop for AttachedDataspace {
    fn drop(&mut self) {
        let err = unsafe { l4re_rm_detach(self.ptr as *mut c_void) };
        if err < 0 {
            Err(Error::from_ipc(err.into())).unwrap()
        }
    }
}

impl Dataspace {
    pub fn alloc(size: usize, flags: MaFlags) -> Result<OwnedCap<Dataspace>> {
        let cap = unsafe { l4re_util_cap_alloc() };
        let err = unsafe { l4re_ma_alloc(size, cap, flags.bits() as u64) };
        if err != 0 {
            Err(Error::from_ipc(err.into()))
        } else {
            Ok(OwnedCap(Cap::from_raw(Dataspace {
                cap,
                size
            })))
        }
    }

    pub fn attach(space: OwnedCap<Dataspace>, map_flags: DsMapFlags, attach_flags: DsAttachFlags) -> Result<AttachedDataspace> {
        let mut vaddr = core::ptr::null_mut();
        let err = unsafe { l4re_rm_attach(
            &mut vaddr as *mut *mut c_void,
            space.size as u64,
            map_flags.bits() as u64 | attach_flags.bits() as u64,
            space.cap,
            0,
            L4_PAGESHIFT as u8
        ) };

        if err != L4_EOK as i32 {
            Err(Error::from_ipc(err.into()))
        } else {
            Ok(AttachedDataspace {
                cap: space,
                ptr: vaddr as *mut u8
            })
        }
    }
}

impl AttachedDataspace {
    pub fn map_dma(&mut self, target: &DmaSpace) -> Result<usize> {
        // TODO L4 only allows us to map 2^n pages at once, if your size
        // is mis-aligned relative to that we have to do multiple maps,
        // for now just panic if we have an invalid mapping size

        // stanford bithacks, check if number is power of 2, if it is find first set bit
        let is_pow_2 = (self.cap.size & (self.cap.size - 1)) == 0;
        if !is_pow_2 {
            panic!("Size of memory space is not a power of two: {}", self.cap.size);
        }
        if self.cap.size < 1 << L4_LOG2_PAGESIZE {
            panic!("Size of memory space is smaller than page size: {}", self.cap.size);
        }
        let mut sz = self.cap.size;
        let mut log2_size = 0;
        while sz != 0 {
            log2_size += 1;
            sz >>= 1;
        }
        // Here self.ptr is in our address space
        let fp = unsafe { l4_fpage_w(self.ptr as usize, log2_size, L4_FPAGE_RWX as u8) };
        // This self.ptr is the address we want to use in the DMA one
        // for simplicity we just use the same
        let ctl = l4_map_obj_control(self.ptr as usize as u64, L4_MAP_ITEM_MAP as u64);
        let tag : MsgTag = unsafe { l4_task_map(target.raw(), THIS_TASK.raw(), fp, ctl as usize) }.into();
        tag.result()?;
        Ok(self.ptr as usize)
    }

    pub fn at_offset<'a>(&'a mut self, offset: usize) -> OffsetAttachedDataspace<'a> {
        OffsetAttachedDataspace {
            ptr: unsafe { self.ptr.add(offset) },
            _space: PhantomData
        }
    }
}

impl VolatileMemoryInterface for AttachedDataspace {
    fn ptr(&self) -> *mut u8 {
        self.ptr
    }
}

impl<'a> VolatileMemoryInterface for OffsetAttachedDataspace<'a> {
    fn ptr(&self) -> *mut u8 {
        self.ptr
    }
}
