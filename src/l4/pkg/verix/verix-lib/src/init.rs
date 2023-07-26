use crate::dev;
use crate::types::{Error, Result, Device, Interrupts};

use log::info;

use pc_hal::prelude::*;
use pc_hal_util::pci::map_bar;

impl<E, IM, PD, D, Res> Device<E, IM, PD, D, Res>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    IM: pc_hal::traits::IoMem<Error=E>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>
{
    
    pub fn init<B, MM, Dma, ICU>(
        vbus: &mut B,
        num_rx_queues: u16,
        num_tx_queues: u16,
        interrupt_timeout: i16,
    ) -> Result<Device<E, IM, PD, D, Res>, E>
    where
        B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
        MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
        Dma: pc_hal::traits::DmaSpace,
        ICU: pc_hal::traits::Icu<Bus=B, Device=D, Error=E>,
    {
        // TODO: this should be reworked to support actual device discovery
        let mut devices = vbus.device_iter();
        let l40009 = devices.next().unwrap();
        let icu = ICU::from_device(&vbus, l40009)?;
        let _pci_bridge = devices.next().unwrap();
        let mut nic = PD::try_of_device(devices.next().unwrap()).unwrap();
        info!("Obtained handles to ICU and NIC");

        let vendor_id = nic.read16(0x0)?;
        let device_id = nic.read16(0x2)?;
        info!("NIC has vendor ID: 0x{:x}, device ID: 0x{:x}", vendor_id, device_id);

        if vendor_id != dev::VENDOR_ID || device_id != dev::DEVICE_ID {
            return Err(Error::IncompatibleDevice);
        }

        if !(num_rx_queues > 0 && num_rx_queues <= dev::MAX_QUEUES) || !(num_tx_queues > 0 && num_tx_queues <= dev::MAX_QUEUES) {
            return Err(Error::InvalidQueueCount);
        }

        let bar0_mem : IM = map_bar(&mut nic, 0)?;
        let mut bar0 = dev::Intel82559ES::Bar0::new(bar0_mem);

        let rx_queues = Vec::with_capacity(num_rx_queues as usize);
        let tx_queues = Vec::with_capacity(num_tx_queues as usize);

        let mut dev = Device {
            bar0,
            num_rx_queues,
            num_tx_queues,
            rx_queues,
            tx_queues,
            device: nic,
            interrupts: Interrupts {
                enabled: false
            }
        };

        // TODO some vfio based interrupt setup happens here. but it is optinal, investigate

        dev.reset_and_init()?;

        Ok(dev)
    }

    pub fn reset_and_init(&mut self) -> Result<(), E> {
        info!("Resetting device");
        // section 4.6.3.1 - disable all interrupts
        self.disable_interrupts();
        Ok(())
    }

    fn clear_interrupts(&mut self) {
        // 31 ones, the last bit in the register is reserved
        self.bar0.eimc().write(|w| w.interrupt_mask(0b1111111111111111111111111111111));
        self.bar0.eicr().read();
    }

    fn disable_interrupts(&mut self) {
        self.bar0.eims().write(|w| w.interrupt_enable(0x0));
        self.clear_interrupts();
    }
}
