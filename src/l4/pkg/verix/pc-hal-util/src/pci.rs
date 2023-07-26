use log::info;
use pc_hal::traits::{IoMemFlags, MaFlags, DsMapFlags, DsAttachFlags};

pub fn map_bar<E, D, IM>(dev: &mut D, bar_idx: u8) -> Result<IM, E>
where
    D: pc_hal::traits::PciDevice<Error=E>,
    IM: pc_hal::traits::IoMem<Error=E>
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
    info!("Mapping BAR{}, addr: 0x{:x}, size: 0x{:x}", bar_idx, bar, size);

    let mut command_reg = dev.read16(0x4)?;
    // enable memory BARs again
    command_reg |= 0b10;
    dev.write16(0x4, command_reg)?;

    IM::request(
        bar as u64,
        size as u64,
        IoMemFlags::EAGER_MAP | IoMemFlags::NONCACHED,
    )
}
