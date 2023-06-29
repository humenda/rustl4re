use l4::cap::{IfaceInit, CapIdx, Interface};
use l4::ipc::MsgTag;
use l4::{Error, Result};
use l4_sys::l4_error_code_t::L4_EOK;
use l4_sys::{l4_icu_bind_w, l4_icu_info_t, l4_icu_info_w, l4_icu_msi_info_t, l4_icu_msi_info_w, l4_icu_unmask_w, l4_timeout_t, l4_uint32_t,l4vbus_get_next_device ,l4vbus_get_resource ,l4vbus_pcidev_cfg_read ,l4vbus_pcidev_cfg_write ,l4vbus_resource_t ,l4vbus_vicu_get_cap ,L4VBUS_NULL ,L4VBUS_ROOT_BUS ,l4_cap_idx_t, l4vbus_device_handle_t};

use l4_sys::l4vbus_consts_t::L4VBUS_MAX_DEPTH;
use l4_sys::l4vbus_icu_src_types::L4VBUS_ICU_SRC_DEV_HANDLE;
use l4_sys::L4_icu_flags::L4_ICU_FLAG_MSI;
use bitflags::bitflags;

use crate::factory::IrqSender;
use crate::sys::l4io_iomem_flags_t::*;
use crate::sys::{l4io_release_iomem, l4io_request_iomem};
use crate::OwnedCap;

use core::iter::Iterator;

pub struct Vbus {
    cap: l4_cap_idx_t,
}

impl Interface for Vbus {
    #[inline]
    fn raw(&self) -> CapIdx {
        self.cap
    }
}

impl IfaceInit for Vbus {
    #[inline]
    fn new(cap: CapIdx) -> Self {
        Self { cap }
    }
}

pub struct Device {
    handle: l4vbus_device_handle_t,
}

pub struct Resource {
    res: l4vbus_resource_t,
}

pub struct DeviceIter<'a> {
    curr: Device,
    vbus: &'a Vbus,
}

pub struct ResourceIter<'a> {
    dev: &'a Device,
    vbus: &'a Vbus,
    idx: i32, // TODO: correct width
}

impl Iterator for DeviceIter<'_> {
    type Item = Device;

    fn next(&mut self) -> Option<Self::Item> {
        let mut next_dev = self.curr.handle;
        let err = unsafe {
            l4vbus_get_next_device(
                self.vbus.raw(),
                L4VBUS_ROOT_BUS as l4vbus_device_handle_t,
                &mut next_dev as *mut l4vbus_device_handle_t,
                L4VBUS_MAX_DEPTH as i32,
                core::ptr::null_mut(),
            )
        };
        if err != L4_EOK as i32 {
            return None;
        }

        self.curr.handle = next_dev;

        Some(Device { handle: next_dev })
    }
}

impl Iterator for ResourceIter<'_> {
    type Item = Resource;

    fn next(&mut self) -> Option<Self::Item> {
        self.idx += 1;
        let mut res: l4vbus_resource_t = unsafe { core::mem::zeroed() };
        let err = unsafe {
            l4vbus_get_resource(
                self.vbus.raw(),
                self.dev.handle,
                self.idx,
                &mut res as *mut l4vbus_resource_t,
            )
        };

        if err != L4_EOK as i32 {
            return None;
        }

        Some(Resource { res })
    }
}

// TODO: Figure the Cap stuff out
impl Vbus {
    pub fn new(cap: l4_cap_idx_t) -> Vbus {
        Vbus { cap }
    }

    pub fn device_iter(&self) -> DeviceIter<'_> {
        DeviceIter {
            curr: Device {
                handle: L4VBUS_NULL as i64,
            },
            vbus: self,
        }
    }

    fn pcidev_cfg_read(&self, dev: &Device, reg: l4_uint32_t, bits: l4_uint32_t) -> Result<u32> {
        let mut res: l4_uint32_t = 0;
        let err = unsafe {
            l4vbus_pcidev_cfg_read(
                self.raw(),
                dev.handle,
                reg,
                &mut res as *mut l4_uint32_t,
                bits,
            )
        };
        if err != L4_EOK as i32 {
            Err(Error::from_ipc(err.into()))
        } else {
            Ok(res)
        }
    }

    fn pcidev_cfg_write(
        &self,
        dev: &Device,
        reg: l4_uint32_t,
        bits: l4_uint32_t,
        val: l4_uint32_t,
    ) -> Result<()> {
        let err = unsafe { l4vbus_pcidev_cfg_write(self.raw(), dev.handle, reg, val, bits) };
        if err != L4_EOK as i32 {
            Err(Error::from_ipc(err.into()))
        } else {
            Ok(())
        }
    }

    pub fn pcidev_cfg_read8(&self, dev: &Device, reg: l4_uint32_t) -> Result<u8> {
        Ok(self.pcidev_cfg_read(dev, reg, 8)? as u8)
    }

    pub fn pcidev_cfg_read16(&self, dev: &Device, reg: l4_uint32_t) -> Result<u16> {
        Ok(self.pcidev_cfg_read(dev, reg, 16)? as u16)
    }

    pub fn pcidev_cfg_read32(&self, dev: &Device, reg: l4_uint32_t) -> Result<u32> {
        self.pcidev_cfg_read(dev, reg, 32)
    }

    pub fn pcidev_cfg_write8(&self, dev: &Device, reg: l4_uint32_t, val: u8) -> Result<()> {
        self.pcidev_cfg_write(dev, reg, val as u32, 8)
    }

    pub fn pcidev_cfg_write16(&self, dev: &Device, reg: l4_uint32_t, val: u16) -> Result<()> {
        self.pcidev_cfg_write(dev, reg, val as u32, 16)
    }

    pub fn pcidev_cfg_write32(&self, dev: &Device, reg: l4_uint32_t, val: u16) -> Result<()> {
        self.pcidev_cfg_write(dev, reg, val as u32, 32)
    }
}

impl Device {
    pub fn resource_iter<'a>(&'a self, vbus: &'a Vbus) -> ResourceIter<'a> {
        ResourceIter {
            dev: self,
            vbus,
            idx: -1,
        }
    }
}

impl Resource {}

pub struct IoMem {
    addr: *mut u8,
    size: u64,
}

bitflags! {
    pub struct IoMemFlags : i32 {
        const NONCACHED = L4IO_MEM_NONCACHED as i32;
        const CACHED = L4IO_MEM_CACHED as i32;
        const USE_MTRR = L4IO_MEM_USE_MTRR as i32;
        const EAGER_MAP = L4IO_MEM_EAGER_MAP as i32;
    }
}

impl IoMem {
    pub fn request(phys: u64, size: u64, flags: IoMemFlags) -> Result<IoMem> {
        let mut virt: u64 = 0;
        let err =
            unsafe { l4io_request_iomem(phys, size, flags.bits() as i32, &mut virt as *mut u64) };
        if err != 0 {
            Err(Error::from_ipc(err.into()))
        } else {
            Ok(IoMem {
                addr: virt as *mut u8,
                size,
            })
        }
    }

    fn release(&mut self) -> Result<()> {
        let err = unsafe { l4io_release_iomem(self.addr as u64, self.size) };
        if err != 0 {
            Err(Error::from_ipc(err.into()))
        } else {
            Ok(())
        }
    }

    fn bounds_check(&self, offset: usize, len: usize) {
        if len + offset > self.size as usize {
            panic!(
                "Iomem out of bounds access, base pointer {:p}, offset: {}, amount: {}",
                self.addr, offset, len
            );
        }
    }

    pub fn write8(&mut self, offset: usize, val: u8) {
        self.bounds_check(offset, 1);
        unsafe { core::ptr::write_volatile(self.addr.add(offset), val) }
    }

    pub fn write16(&mut self, offset: usize, val: u16) {
        self.bounds_check(offset, 2);
        unsafe { core::ptr::write_volatile(self.addr.add(offset) as *mut u16, val) }
    }

    pub fn write32(&mut self, offset: usize, val: u32) {
        self.bounds_check(offset, 4);
        unsafe { core::ptr::write_volatile(self.addr.add(offset) as *mut u32, val) }
    }

    pub fn write64(&mut self, offset: usize, val: u64) {
        self.bounds_check(offset, 8);
        unsafe { core::ptr::write_volatile(self.addr.add(offset) as *mut u64, val) }
    }

    pub fn read8(&self, offset: usize) -> u8 {
        self.bounds_check(offset, 1);
        unsafe { core::ptr::read_volatile(self.addr.add(offset)) }
    }

    pub fn read16(&self, offset: usize) -> u16 {
        self.bounds_check(offset, 2);
        unsafe { core::ptr::read_volatile(self.addr.add(offset) as *mut u16) }
    }

    pub fn read32(&self, offset: usize) -> u32 {
        self.bounds_check(offset, 4);
        unsafe { core::ptr::read_volatile(self.addr.add(offset) as *mut u32) }
    }

    pub fn read64(&self, offset: usize) -> u64 {
        self.bounds_check(offset, 8);
        unsafe { core::ptr::read_volatile(self.addr.add(offset) as *mut u64) }
    }
}

impl Drop for IoMem {
    fn drop(&mut self) {
        self.release().unwrap();
    }
}

pub struct Icu {
    cap: l4_cap_idx_t,
}

#[derive(Copy, Clone, Debug)]
pub struct IcuInfo {
    pub has_msi: bool,
    pub nr_irqs: u32,
    pub nr_msis: u32,
}

pub struct MsiInfo {
    pub addr: u64,
    pub data: u32,
}

impl Interface for Icu {
    #[inline]
    fn raw(&self) -> CapIdx {
        self.cap
    }
}

impl IfaceInit for Icu {
    #[inline]
    fn new(cap: CapIdx) -> Self {
        Self { cap }
    }
}

impl Icu {
    pub fn from_device(vbus: &Vbus, dev: Device) -> Result<OwnedCap<Self>> {
        let cap: OwnedCap<Self> = OwnedCap::alloc();
        let err = unsafe { l4vbus_vicu_get_cap(vbus.raw(), dev.handle, cap.raw()) };
        if err != L4_EOK as i32 {
            Err(Error::from_ipc(err.into()))
        } else {
            Ok(cap)
        }
    }

    pub fn info(&self) -> Result<IcuInfo> {
        let mut info: l4_icu_info_t = unsafe { core::mem::zeroed() };
        let tag: MsgTag =
            unsafe { l4_icu_info_w(self.cap, &mut info as *mut l4_icu_info_t) }.into();
        tag.result()?;
        Ok(IcuInfo {
            has_msi: info.features != 0,
            nr_irqs: info.nr_irqs,
            nr_msis: info.nr_msis,
        })
    }

    /// Returns the MSI info and whether to unmask via the ICU or not
    pub fn bind_msi(
        &self,
        irq_cap: &IrqSender,
        irq_num: u32,
        dev: &Device,
    ) -> Result<(MsiInfo, bool)> {
        let final_irq = irq_num | L4_ICU_FLAG_MSI as u32;
        let tag: MsgTag = unsafe { l4_icu_bind_w(self.cap, final_irq, irq_cap.raw()) }.into();
        let label = tag.label();
        tag.result()?;
        let mut msi_info_c: l4_icu_msi_info_t = unsafe { core::mem::zeroed() };
        let tag: MsgTag = unsafe {
            l4_icu_msi_info_w(
                self.cap,
                final_irq,
                (dev.handle | L4VBUS_ICU_SRC_DEV_HANDLE as i64) as u64,
                &mut msi_info_c as *mut l4_icu_msi_info_t,
            )
        }
        .into();
        tag.result()?;
        let msi_info = MsiInfo {
            addr: msi_info_c.msi_addr,
            data: msi_info_c.msi_data,
        };

        if label == 0 {
            Ok((msi_info, false))
        } else if label == 1 {
            Ok((msi_info, true))
        } else {
            panic!("Invalid return label from l4_icu_bind: {}", label)
        }
    }

    pub fn unmask(&self, irq_num: u32) -> Result<()> {
        let tag: MsgTag = unsafe {
            l4_icu_unmask_w(
                self.cap,
                irq_num,
                core::ptr::null_mut(),
                l4_timeout_t { raw: 0 },
            )
        }
        .into();
        tag.result()?;
        Ok(())
    }
}
