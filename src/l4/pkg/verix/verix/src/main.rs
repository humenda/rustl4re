extern crate l4;

use pc_hal_l4::{Vbus, PciDevice, DmaSpace, MappableMemory, Device, Resource, Icu, IoMem, IrqHandler};
use pc_hal::traits::Bus;
use verix_lib::run;

fn main() {
    simple_logger::SimpleLogger::new().init().unwrap();
    let vbus = Vbus::get().unwrap();
    run::<l4::Error, Device, PciDevice, Vbus, Resource, MappableMemory, DmaSpace, Icu, IoMem, IrqHandler>(vbus).unwrap();
}
