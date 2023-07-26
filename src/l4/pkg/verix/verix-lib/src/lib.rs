#[macro_use]
extern crate pc_hal_util;

use pc_hal::prelude::*;

mod dev;
pub mod init;
pub mod types;

use crate::types::Result;


/*
fn do_dma<E, MappableMemory, Dma, Res, B, Dev>(vbus: &B, nic: &Dev) -> Result<MappableMemory, E> where
    Dma: pc_hal::traits::DmaSpace,
    Res: pc_hal::traits::Resource,
    B: pc_hal::traits::Bus<Error=E, Resource=Res, DmaSpace=Dma>,
    Dev: pc_hal::traits::PciDevice<Error=E, Resource=Res>,
    MappableMemory: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
{
    let mut dma_space = Dma::new();
    let mut dma_domain = nic.resource_iter().filter(|r| r.typ() == ResourceType::DmaDomain).next().unwrap();
    println!("Found DMA domain");
    println!("Assigning");
    vbus.assign_dma_domain(&mut dma_domain, &mut dma_space)?;
    println!("Assigned DMA domain");

    let size: usize = 4096;
    println!("Allocating DMA memory");
    let mut mem = MappableMemory::alloc(
        size,
        MaFlags::PINNED | MaFlags::CONTINUOUS,
        DsMapFlags::RW,
        DsAttachFlags::SEARCH_ADDR | DsAttachFlags::EAGER_MAP
    )?;

    println!("Mapping space");
    let dma_addr = mem.map_dma(&dma_space)?;
    println!("Mapped space to dma virtual memory at 0x{:x}!!!", dma_addr);
    println!("Writing some data to DMA space to check");
    mem.write32(0x0, 0xdead);
    let res = mem.read32(0x0);
    println!("Wrote: {}, Read: {}", 0xdead, res);
    Ok(mem)
}

fn map_bar<E, D, IM>(nic: &mut D, bar_idx: u8) -> Result<IM, E>
where
    D: pc_hal::traits::PciDevice<Error=E>,
    IM: pc_hal::traits::IoMem<Error=E>
{
    // TODO: 64 bit BAR
    let mut command_reg = nic.read16(0x4)?;
    // disable memory BARs
    command_reg &= !0b10;
    nic.write16(0x4, command_reg)?;

    let bar_addr = (bar_idx as u32) * 4 + 0x10;
    let bar_reg = nic.read32(bar_addr)?;
    let bar = bar_reg & 0xFFFFFFF0;
    nic.write32(bar_addr, 0xFFFFFFFF)?;
    let mut size = nic.read32(bar_addr)?;
    size = size & 0xFFFFFFF0;
    size = !size;
    size += 1;
    nic.write32(bar_addr, bar_reg)?;
    println!("Mapping BAR{}, addr: 0x{:x}, size: 0x{:x}", bar_idx, bar, size);

    let mut command_reg = nic.read16(0x4)?;
    // enable memory BARs again
    command_reg |= 0b10;
    nic.write16(0x4, command_reg)?;

    IM::request(
        bar as u64,
        size as u64,
        IoMemFlags::EAGER_MAP | IoMemFlags::NONCACHED,
    )
}

fn print_pci<E, D: pc_hal::traits::PciDevice<Error=E>>(nic: &mut D) -> Result<(), E> {
    let vendor_id = nic.read16(0x0)?;
    let device_id = nic.read16(0x2)?;
    let revision_id = nic.read8(0x8)?;
    println!("Read from PCI");
    println!(
        "vendor: {:#x}, device: {:#x}, revision:{:#x}",
        vendor_id, device_id, revision_id
    );

    let mut command_reg = nic.read16(0x4)?;
    println!("Command reg: 0x{:x}", command_reg);
    // enabled memory BARs
    command_reg |= 0b10;
    nic.write16(0x4, command_reg)?;
    Ok(())
}

struct CapResult {
    id: u8,
    next: u8,
    ptr: u8,
}

fn parse_cap<E, D: pc_hal::traits::PciDevice<Error=E>>(nic: &mut D, curr_cap_ptr: u8) -> Result<CapResult, E> {
    let id = nic.read8(curr_cap_ptr.into())?;
    let next = nic.read8((curr_cap_ptr + 1).into())?;

    Ok(CapResult {
        id,
        next,
        ptr: curr_cap_ptr,
    })
}

fn find_msix_cap<E, D: pc_hal::traits::PciDevice<Error=E>>(nic: &mut D, initial_cap_ptr: u8) -> Result<CapResult, E> {
    let mut cap_ptr = initial_cap_ptr;
    loop {
        let res = parse_cap(nic, cap_ptr)?;
        if res.id == 0x11 {
            return Ok(res);
        } else if res.next == 0 {
            panic!("Traversed to end of list and did not find MSI-X cap");
        }
        cap_ptr = res.next;
    }
}

fn enable_msix<E, D: pc_hal::traits::PciDevice<Error=E>>(nic: &mut D, msix_cap: u8) -> Result<(), E> {
    let mut message_control = nic.read16((msix_cap + 2).into())?;
    println!("Read message control register: 0x{:x}", message_control);
    message_control |= 1 << 15; // set enable bit
    message_control &= !(1 << 14); // unset masking bit
    println!("Writing message control register: 0x{:x}", message_control);
    nic.write16((msix_cap + 2).into(), message_control)?;
    Ok(())
}

fn register_msix<E, PD, B, D, ICU, IM>(nic: &mut PD, icu: ICU) -> Result<(), E>
where
    // TODO: unclear whether this AsMut bound should be here or more of a general thing
    PD: pc_hal::traits::PciDevice<Error=E> + AsMut<D>,
    B: pc_hal::traits::Bus<Error=E>,
    D: pc_hal::traits::Device,
    ICU: pc_hal::traits::Icu<Bus=B, Device=D, Error=E>,
    IM: pc_hal::traits::IoMem<Error=E>
{
    let status_register = nic.read16(0x6)?;
    println!("Status register: 0x{:x}", status_register);
    let has_capabilities = (status_register & (1 << 4)) != 0;
    if !has_capabilities {
        panic!("No capability list support");
    }
    println!("Status register indicates capability list is supported");
    let cap_ptr = nic.read8(0x34)?;
    println!(
        "Found initial entry to capability list at 0x{:x}, searching MSI-X cap",
        cap_ptr
    );
    let msix_cap = find_msix_cap(nic, cap_ptr)?.ptr;
    println!("Found MSI-X capability at 0x{:x}", msix_cap);
    println!("Enabling MSI-X");
    enable_msix(nic, msix_cap)?;
    println!("MSI-X enabled");

    // And now we set up the interrupt handler
    println!("Binding irq and obtaining MSI info");
    let irq = 0;
    let nic_dev : &mut D = nic.as_mut();
    assert!(icu.nr_msis()? > 0);
    let mut irq_handler = icu.bind_msi(irq, nic_dev)?;
    println!("registering the MSI interrupt on the device");
    let table_offset_reg = nic.read32((msix_cap + 0x4).into())?;
    let bir : u8 = (table_offset_reg & 0b111) as u8;
    let table_offset = (table_offset_reg & !(0b111)) as usize;
    let mut msix_table : IM = map_bar(nic, bir)?;
    // 0 * 16 is the slot we are using since we are going for interrupt 0
    msix_table.write32(table_offset + 0 * 16 + 0, (irq_handler.msi_addr() & 0xffffffff) as u32);
    msix_table.write32(table_offset + 0 * 16 + 4, (irq_handler.msi_addr() >> 32) as u32);
    msix_table.write32(table_offset + 0 * 16 + 8, irq_handler.msi_data());
    let mask_reg = msix_table.read32(table_offset + 0 * 16 + 12);
    msix_table.write32(table_offset + 0 * 16 + 12, mask_reg | 1);

    icu.unmask(&mut irq_handler)?;
    println!("Done with MSI-X");
    // TODO: PCI bus mastering
    Ok(())
}

fn test_gen<E, PD, IM>(nic: &mut PD) -> Result<(), E>
where
    PD: pc_hal::traits::PciDevice<Error=E>,
    IM: pc_hal::traits::IoMem<Error=E>
{
    let bar0_mem : IM = map_bar(nic, 0)?;
    let mut bar0 = dev::Intel82559ES::Bar0::new(bar0_mem);
    let mut ledctl = bar0.ledctl().read();
    println!(
        "led1_mode (should be 1): 0x{:x}, led2_mode (should be 4) 0x{:x}",
        ledctl.led1_mode(),
        ledctl.led2_mode(),
    );
    println!("Switching led2_mode to 1");
    bar0.ledctl().modify(|_, w| w.led2_mode(1));
    ledctl = bar0.ledctl().read();
    println!(
        "led1_mode (should be 1): 0x{:x}, led2_mode (should be 1) 0x{:x}",
        ledctl.led1_mode(),
        ledctl.led2_mode(),
    );
    println!("Switching led2_mode to 4, all other bits to 0");
    bar0.ledctl().write(|w| w.led2_mode(4));
    ledctl = bar0.ledctl().read();
    println!(
        "led1_mode (should be 0): 0x{:x}, led2_mode (should be 1) 0x{:x}",
        ledctl.led1_mode(),
        ledctl.led2_mode(),
    );
    Ok(())
}

pub fn run<E, D, PD, B, Res, MM, Dma, ICU, IM>(vbus : B) -> Result<(), E>
where
    D: pc_hal::traits::Device,
    B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>,
    Res: pc_hal::traits::Resource,
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
    ICU: pc_hal::traits::Icu<Bus=B, Device=D, Error=E>,
    IM: pc_hal::traits::IoMem<Error=E>
{
    println!("Got vbus capability!");
    println!("==================== Looking for ICU and NIC on VBUS ====================");
    let mut devices = vbus.device_iter();
    let l40009 = devices.next().unwrap();
    let icu = ICU::from_device(&vbus, l40009)?;
    let _pci_bridge = devices.next().unwrap();
    let mut nic = PD::try_of_device(devices.next().unwrap()).unwrap();
    println!("==================== Found all devices that I need ====================");
    println!("==================== Printing PCI info and mapping a BAR because we can ====================");
    print_pci(&mut nic)?;
    println!("==================== Registering a random MSI-X interrupt ====================");
    // TODO: it should be possible to write the rust in such a way that this just gets inferred
    register_msix::<_, _, _, _, _, IM>(&mut nic, icu)?;
    println!("==================== Registering a random DMA mapping ====================");
    let mem : MM = do_dma(&vbus, &nic)?;
    println!("==================== Trying out my auto generated code ====================");
    test_gen::<_, _, IM>(&mut nic)?;
    println!("==================== Done with everything ====================");
    Ok(())
}
*/

pub fn run<E, D, PD, B, Res, MM, Dma, ICU, IM>(mut vbus : B) -> Result<(), E>
where
    D: pc_hal::traits::Device,
    B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>,
    Res: pc_hal::traits::Resource,
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
    ICU: pc_hal::traits::Icu<Bus=B, Device=D, Error=E>,
    IM: pc_hal::traits::IoMem<Error=E>
{
    let dev : types::Device<E, IM, PD, D, Res> = types::Device::init::<B, MM, Dma, ICU>(
        &mut vbus,
        1,
        1,
        10 // TODO: choose a good value
    )?;
    Ok(())
}
