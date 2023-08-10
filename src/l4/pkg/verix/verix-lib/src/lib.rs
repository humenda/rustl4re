#[macro_use]
extern crate pc_hal_util;

pub mod constants;
pub mod dev;
pub mod dma;
pub mod init;
pub mod tx;
pub mod types;

use byteorder::{ByteOrder, LittleEndian};
use log::{info, trace};
use std::{collections::VecDeque, time::Instant};

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
    dev.read_stats(&mut dev_stats);

    let mut buffer: VecDeque<dma::Packet<E, Dma, MM>> = VecDeque::with_capacity(BATCH_SIZE);
    let mut time = Instant::now();
    let mut counter = 0;
    dev.log_queue_state(0);

    loop {
        // TODO echo
        let num_rx = dev.rx_batch(0, &mut buffer, BATCH_SIZE);

        if num_rx > 0 {
            trace!("Got packets");

            for p in buffer.iter_mut() {
                trace!("Got: {:?}", p);
            }

            dev.tx_batch(0, &mut buffer);
            dev.log_queue_state(0);

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
                info!("Stats: {:?}", dev_stats);

                time = Instant::now();
            }
        }
        counter += 1
    }
}
