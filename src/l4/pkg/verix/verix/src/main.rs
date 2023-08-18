extern crate l4;

use pc_hal::traits::Bus;
use pc_hal_l4::{Device, DmaSpace, IoMem, MappableMemory, PciDevice, Resource, Vbus};
use verix_lib::run;

fn main() {
    simple_logger::SimpleLogger::new().init().unwrap();
    let vbus = Vbus::get().unwrap();
    run::<l4::Error, Device, PciDevice, Vbus, Resource, MappableMemory, DmaSpace, IoMem>(vbus)
        .unwrap();
}
