#[macro_use]
extern crate pc_hal_util;

pub mod constants;
pub mod dev;
pub mod dma;
pub mod init;
pub mod tx;
pub mod types;

use byteorder::{ByteOrder, LittleEndian};
use log::info;
use std::collections::VecDeque;

use crate::types::Result;
use pc_hal::traits::MemoryInterface;

// number of packets sent simultaneously by our driver
const BATCH_SIZE: usize = 1;
// size of our packets
const PACKET_SIZE: usize = 69;

pub fn run<E, D, PD, B, Res, MM, Dma, ICU, IM, ISR>(mut bus: B) -> Result<(), E>
where
    D: pc_hal::traits::Device,
    B: pc_hal::traits::Bus<Error = E, Device = D, Resource = Res, DmaSpace = Dma>,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res> + AsMut<D>,
    Res: pc_hal::traits::Resource,
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
    ICU: pc_hal::traits::Icu<Bus = B, Device = D, Error = E, IrqHandler = ISR>,
    IM: pc_hal::traits::IoMem<Error = E>,
    ISR: pc_hal::traits::IrqHandler,
{
    //  TODO: proper device discovery
    let mut devices = bus.device_iter();
    let l40009 = devices.next().unwrap();
    let mut icu = ICU::from_device(&bus, l40009)?;
    let _pci_bridge = devices.next().unwrap();
    let nic = PD::try_of_device(devices.next().unwrap()).unwrap();
    info!("Obtained handles to ICU and NIC");

    let mut dev: types::Device<E, IM, PD, D, Res, Dma, MM, ISR> = types::Device::init::<B, ICU>(
        &mut bus, &mut icu, nic, 1, 1, 10, // TODO: choose a good value
    )?;

    let mut dev_stats = Default::default();
    let mut seq_num = 0;
    let mut buffer: VecDeque<dma::Packet<E, Dma, MM>> = VecDeque::with_capacity(BATCH_SIZE);

    dev.read_stats(&mut dev_stats);

    let mut pkt_data = [
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06, // dst MAC
        0x10,
        0x10,
        0x10,
        0x10,
        0x10,
        0x10, // src MAC
        0x08,
        0x00, // ether type: IPv4
        0x45,
        0x00,                              // Version, IHL, TOS
        ((PACKET_SIZE - 14) >> 8) as u8,   // ip len excluding ethernet, high byte
        ((PACKET_SIZE - 14) & 0xFF) as u8, // ip len excluding ethernet, low byte
        0x00,
        0x00,
        0x00,
        0x00, // id, flags, fragmentation
        0x40,
        0x11,
        0x00,
        0x00, // TTL (64), protocol (UDP), checksum
        0x0A,
        0x00,
        0x00,
        0x01, // src ip (10.0.0.1)
        0x0A,
        0x00,
        0x00,
        0x02, // dst ip (10.0.0.2)
        0x00,
        0x2A,
        0x05,
        0x39,                                   // src and dst ports (42 -> 1337)
        ((PACKET_SIZE - 20 - 14) >> 8) as u8,   // udp len excluding ip & ethernet, high byte
        ((PACKET_SIZE - 20 - 14) & 0xFF) as u8, // udp len excluding ip & ethernet, low byte
        0x00,
        0x00, // udp checksum, optional
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
        b'i',
        b'x',
        b'y', // payload
              // rest of the payload is zero-filled because mempools guarantee empty bufs
    ];

    pkt_data[6..12].clone_from_slice(&dev.get_mac_addr());

    let pool = dev.tx_queues[0].pool.clone();
    // re-fill our packet queue with new packets to send out
    dma::alloc_pkt_batch(&pool, &mut buffer, BATCH_SIZE, PACKET_SIZE);

    for p in buffer.iter_mut() {
        for (i, data) in pkt_data.iter().enumerate() {
            p[i] = *data;
        }

        // update sequence number of all packets (and checksum if necessary)
        LittleEndian::write_u32(&mut p[(PACKET_SIZE - 4)..], seq_num);
        seq_num = seq_num.wrapping_add(1);

        let checksum = calc_ipv4_checksum(&p[14..14 + 20]);
        // Calculated checksum is little-endian; checksum field is big-endian
        p[24] = (checksum >> 8) as u8;
        p[25] = (checksum & 0xff) as u8;
    }

    for p in buffer.iter() {
        info!(
            "Sending: {:?}, address: {:p}, 0x{:x}",
            p, p.addr_virt, p.addr_phys
        );
    }

    dev.tx_batch_busy_wait(0, &mut buffer);

    loop {
        std::thread::sleep(std::time::Duration::from_millis(500));
        dev.read_stats(&mut dev_stats);
        let tdbal = dev.bar0.tdbal(0).read().tdbal();
        let tdbah = dev.bar0.tdbah(0).read().tdbah();
        let tdlen = dev.bar0.tdlen(0).read().len();
        let tdh = dev.bar0.tdh(0).read().tdh();
        let tdt = dev.bar0.tdt(0).read().tdt();
        let tx_queue = &mut dev.tx_queues[0];
        let ptr = tx_queue.descriptors.ptr();
        let first = unsafe { core::ptr::read_volatile(ptr as *const u64) };
        let second = unsafe { core::ptr::read_volatile(ptr.add(0x8) as *const u64) };
        info!("descriptor mem: 0x{:x}, 0x{:x}", first, second);
        info!(
            "tdbal 0x{:x}, tdbah 0x{:x}, tdlen: {}, tdh: {}, tdt: {}",
            tdbal, tdbah, tdlen, tdh, tdt
        );
        info!("Stats: {:?}", dev_stats);
    }
}

/// Calculates IPv4 header checksum
fn calc_ipv4_checksum(ipv4_header: &[u8]) -> u16 {
    assert_eq!(ipv4_header.len() % 2, 0);
    let mut checksum = 0;
    for i in 0..ipv4_header.len() / 2 {
        if i == 5 {
            // Assume checksum field is set to 0
            continue;
        }
        checksum += (u32::from(ipv4_header[i * 2]) << 8) + u32::from(ipv4_header[i * 2 + 1]);
        if checksum > 0xffff {
            checksum = (checksum & 0xffff) + 1;
        }
    }
    !(checksum as u16)
}
