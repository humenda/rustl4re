#[macro_use]
extern crate pc_hal_util;

mod dev;
mod constants;
mod dma;
pub mod init;
pub mod types;

use crate::types::Result;

pub fn run<E, D, PD, B, Res, MM, Dma, ICU, IM, ISR>(mut vbus : B) -> Result<(), E>
where
    D: pc_hal::traits::Device,
    B: pc_hal::traits::Bus<Error=E, Device=D, Resource=Res, DmaSpace=Dma>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>,
    Res: pc_hal::traits::Resource,
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
    ICU: pc_hal::traits::Icu<Bus=B, Device=D, Error=E, IrqHandler=ISR>,
    IM: pc_hal::traits::IoMem<Error=E>,
    ISR: pc_hal::traits::IrqHandler
{
    let dev : types::Device<E, IM, PD, D, Res, Dma, MM, ISR> = types::Device::init::<B, ICU>(
        &mut vbus,
        1,
        1,
        10 // TODO: choose a good value
    )?;
    Ok(())
}
