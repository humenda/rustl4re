use log::{info, trace};
use pc_hal::traits::IoMemFlags;

pub fn map_bar<E, D, IM>(dev: &mut D, bar_idx: u8) -> Result<IM, E>
where
    D: pc_hal::traits::PciDevice<IoMem = IM, Error = E>,
{
    // TODO: 64 bit BAR
    let mut command_reg = dev.read16(0x4)?;
    // disable memory BARs
    command_reg &= !0b10;
    dev.write16(0x4, command_reg)?;

    let bar_addr = (bar_idx as u32) * 4 + 0x10;
    let bar_reg = dev.read32(bar_addr)?;
    let bar = bar_reg & 0xFFFFFFF0;
    dev.write32(bar_addr, 0xFFFFFFFF)?;
    let mut size = dev.read32(bar_addr)?;
    size = size & 0xFFFFFFF0;
    size = !size;
    size += 1;
    dev.write32(bar_addr, bar_reg)?;
    info!(
        "Mapping BAR{}, addr: 0x{:x}, size: 0x{:x}",
        bar_idx, bar, size
    );

    let mut command_reg = dev.read16(0x4)?;
    // enable memory BARs again
    command_reg |= 0b10;
    dev.write16(0x4, command_reg)?;

    dev.request_iomem(
        bar as u64,
        size as u64,
        IoMemFlags::EAGER_MAP | IoMemFlags::NONCACHED,
    )
}

struct CapResult {
    id: u8,
    next: u8,
    ptr: u8,
}

fn parse_cap<E, D: pc_hal::traits::PciDevice<Error = E>>(
    dev: &mut D,
    curr_cap_ptr: u8,
) -> Result<CapResult, E> {
    let id = dev.read8(curr_cap_ptr.into())?;
    let next = dev.read8((curr_cap_ptr + 1).into())?;

    Ok(CapResult {
        id,
        next,
        ptr: curr_cap_ptr,
    })
}

fn find_msix_cap<E, D: pc_hal::traits::PciDevice<Error = E>>(
    dev: &mut D,
    initial_cap_ptr: u8,
) -> Result<Option<CapResult>, E> {
    let mut cap_ptr = initial_cap_ptr;
    trace!("Looking for MSI-X capability, starting at 0x{:x}", cap_ptr);
    loop {
        let res = parse_cap(dev, cap_ptr)?;
        trace!("Checking next cap, ID 0x{:x}", res.id);
        if res.id == 0x11 {
            trace!("Found MSI-X cap at 0x{:x}", res.ptr);
            return Ok(Some(res));
        } else if res.next == 0 {
            trace!("Checked all caps, couldn't find MSI-X");
            return Ok(None);
        }
        cap_ptr = res.next;
    }
}

fn enable_msix<E, D: pc_hal::traits::PciDevice<Error = E>>(
    dev: &mut D,
    msix_cap: u8,
) -> Result<(), E> {
    let mut message_control = dev.read16((msix_cap + 2).into())?;
    message_control |= 1 << 15; // set enable bit
    message_control &= !(1 << 14); // unset masking bit
    dev.write16((msix_cap + 2).into(), message_control)?;
    Ok(())
}

pub fn enable_bus_master<E, D: pc_hal::traits::PciDevice<Error = E>>(dev: &mut D) -> Result<(), E> {
    let mut command_reg = dev.read16(0x4)?;
    // enable bus master
    command_reg |= 4;
    dev.write16(0x4, command_reg)?;
    Ok(())
}

pub fn map_msix_cap<E, D, IM>(dev: &mut D) -> Result<Option<IM>, E>
where
    D: pc_hal::traits::PciDevice<IoMem = IM, Error = E>,
{
    let status_register = dev.read16(0x6)?;
    let has_capabilities = (status_register & (1 << 4)) != 0;
    if !has_capabilities {
        trace!("Device does not support capability list");
        return Ok(None);
    }
    let cap_ptr = dev.read8(0x34)?;
    if let Some(msix_cap) = find_msix_cap(dev, cap_ptr)? {
        enable_msix(dev, msix_cap.ptr)?;
        let table_offset_reg = dev.read32((msix_cap.ptr + 0x4).into())?;
        let bir = (table_offset_reg & 0b111) as u8;
        let msix_table = map_bar(dev, bir)?;
        Ok(Some(msix_table))
    } else {
        Ok(None)
    }
}
