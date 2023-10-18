#![allow(non_snake_case)]
#[macro_use]
extern crate pc_hal_util;

pub mod constants;
pub mod dev;
pub mod dma;
pub mod init;
pub mod tx;
pub mod types;

use log::info;
use std::{collections::VecDeque, time::Instant};

use crate::types::Result;

// number of packets sent simultaneously by our driver

pub fn find_dev<E, D, PD, B, Res, Dma, IM>(
    bus: &mut B,
) -> Result<types::UninitializedDevice<E, IM, PD, D, Res, Dma>, E>
where
    D: pc_hal::traits::Device,
    B: pc_hal::traits::Bus<Error = E, Device = D, Resource = Res, DmaSpace = Dma>,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res, IoMem = IM>,
    Res: pc_hal::traits::Resource,
    Dma: pc_hal::traits::DmaSpace,
    IM: pc_hal::traits::MemoryInterface,
{
    let mut devices = bus.device_iter();
    let _l40009 = devices.next().unwrap();
    let _pci_bridge = devices.next().unwrap();
    let nic = PD::try_of_device(devices.next().unwrap()).unwrap();
    info!("Obtained handles to NIC");

    let dev: types::UninitializedDevice<E, IM, PD, D, Res, Dma> =
        types::UninitializedDevice::new(bus, nic)?;
    Ok(dev)
}

pub fn run<E, D, PD, B, Res, MM, Dma, IM>(bus: &mut B) -> Result<(), E>
where
    D: pc_hal::traits::Device,
    B: pc_hal::traits::Bus<Error = E, Device = D, Resource = Res, DmaSpace = Dma>,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res, IoMem = IM>,
    Res: pc_hal::traits::Resource,
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
    IM: pc_hal::traits::MemoryInterface,
{
    let uninit_dev: types::UninitializedDevice<E, IM, PD, D, Res, Dma> = find_dev(bus)?;
    let dev = uninit_dev.init()?;
    let mut dev_stats = Default::default();
    let mut dev_stats_old = Default::default();
    dev.read_stats(&mut dev_stats);
    dev.read_stats(&mut dev_stats_old);

    let mut buffer: VecDeque<dma::Packet<E, Dma, MM>> =
        VecDeque::with_capacity(constants::BATCH_SIZE);
    let mut time = Instant::now();
    let mut counter = 0;

    loop {
        let num_rx = dev.rx_batch(&mut buffer, constants::BATCH_SIZE);

        if num_rx > 0 {
            for pkt in buffer.iter_mut() {
                for idx in 0..6 {
                    pkt.swap(idx, idx + 6);
                }
            }

            dev.tx_batch(&mut buffer, constants::BATCH_SIZE) as u64;

            // drop packets if they haven't been sent out
            buffer.drain(..);
        }

        // don't poll the time unnecessarily
        if counter & 0xfff == 0 {
            let elapsed = time.elapsed();
            let nanos = elapsed.as_secs() * 1_000_000_000 + u64::from(elapsed.subsec_nanos());
            // every second
            if nanos > 1_000_000_000 {
                dev.read_stats(&mut dev_stats);
                dev_stats.print_stats_diff(&dev_stats_old, nanos);
                dev_stats_old = dev_stats;
                time = Instant::now();
            }
        }
        counter += 1
    }
}
