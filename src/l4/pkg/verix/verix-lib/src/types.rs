use crate::dev;

#[derive(Debug)]
pub enum Error<E> {
    HalError(E),
    /// The driver noticed the given device was incompatible
    IncompatibleDevice,
    /// The driver was told to create an invalid amount of queues
    InvalidQueueCount,
    /// The driver did not manage to bring the link up
    LinkDown
}

impl<E> From<E> for Error<E> {
    fn from(value: E) -> Self {
        Self::HalError(value)
    }
}

pub type Result<T, E> = std::result::Result<T, Error<E>>;

pub struct Device<E, IM, PD, D, Res>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    IM: pc_hal::traits::IoMem<Error=E>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>
{
    pub (crate) bar0: dev::Intel82559ES::Bar0<IM>,
    pub (crate) device: PD,
    pub (crate) num_rx_queues: u16,
    pub (crate) num_tx_queues: u16,
    pub (crate) rx_queues: Vec<RxQueue>,
    pub (crate) tx_queues: Vec<TxQueue>,
    pub (crate) interrupts: Interrupts,
}

pub struct RxQueue {}
pub struct TxQueue {}
pub struct Interrupts {
    pub (crate) enabled : bool
}
