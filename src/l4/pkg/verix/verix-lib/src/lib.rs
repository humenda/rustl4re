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
const BATCH_SIZE: usize = 64;

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
    let mut dev_stats_old = Default::default();
    dev.read_stats(&mut dev_stats);
    dev.read_stats(&mut dev_stats_old);

    let mut buffer: VecDeque<dma::Packet<E, Dma, MM>> = VecDeque::with_capacity(BATCH_SIZE);
    let mut time = Instant::now();
    let mut counter = 0;

    let mut tx_rdtsc: u64= 0;
    let mut tx_count: u64 = 0;

    let mut rx_count: u64 = 0;
    let mut rx_pkt_rdtsc: u64= 0;
    let mut rx_idle_rdtsc : u64 = 0;
    let mut rx_idle_count : u64 = 0;

    let mut proc_count : u64 = 0;
    let mut proc_rdtsc : u64 = 0;

    loop {
        let rx_start = unsafe { core::arch::x86_64::_rdtsc() };
        let num_rx = dev.rx_batch(0, &mut buffer, BATCH_SIZE);
        let rx_end = unsafe { core::arch::x86_64::_rdtsc() };

        if num_rx > 0 {
            rx_pkt_rdtsc += rx_end - rx_start;
            rx_count += num_rx as u64;


            let proc_start = unsafe { core::arch::x86_64::_rdtsc() };
            for pkt in buffer.iter_mut() {
                for idx in 0..6 {
                    pkt.swap(idx, idx+6);
                }
            }
            let proc_end = unsafe { core::arch::x86_64::_rdtsc() };
            proc_rdtsc += proc_end - proc_start;
            proc_count += num_rx as u64;

            let tx_start = unsafe { core::arch::x86_64::_rdtsc() };
            tx_count += dev.tx_batch(0, &mut buffer) as u64;
            let tx_end = unsafe { core::arch::x86_64::_rdtsc() };
            tx_rdtsc += tx_end - tx_start;

            // drop packets if they haven't been sent out
            buffer.drain(..);
        } else {
            rx_idle_rdtsc += rx_end - rx_start;
            rx_idle_count += 1;

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

                let tx_rdtsc_pp = tx_rdtsc.checked_div(tx_count).unwrap_or(0);
                let rx_rdtsc_pp = rx_pkt_rdtsc.checked_div(rx_count).unwrap_or(0);
                let rx_rdtsc_idle = rx_idle_rdtsc.checked_div(rx_idle_count).unwrap_or(0);
                let proc_rdtsc_pp = proc_rdtsc.checked_div(proc_count).unwrap_or(0);
                info!("TX PP: {}, RX PP: {}, RX Idle/Iter: {}, Proc PP: {}", tx_rdtsc_pp, rx_rdtsc_pp, rx_rdtsc_idle, proc_rdtsc_pp);
                time = Instant::now();
            }
        }
        counter += 1
    }
}
