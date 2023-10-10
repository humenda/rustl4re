use std::{collections::VecDeque, cell::RefCell};

use crate::{
    dev,
    dma::{DmaMemory, Mempool},
};

#[derive(Debug)]
pub enum Error<E> {
    HalError(E),
    /// The driver noticed the given device was incompatible
    IncompatibleDevice,
    /// The driver was told to create an invalid amount of queues
    InvalidQueueCount,
    /// The driver did not manage to bring the link up
    LinkDown,
    /// The driver noticed that MSI-X is not supported on the device
    MsixMissing,
    /// No more pieces left in a mempool
    MempoolEmpty,
}

impl<E> From<E> for Error<E> {
    fn from(value: E) -> Self {
        Self::HalError(value)
    }
}

pub type Result<T, E> = std::result::Result<T, Error<E>>;

pub struct UninitializedDevice<E, IM, PD, D, Res, Dma>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res, IoMem = IM>,
    Dma: pc_hal::traits::DmaSpace,
    IM: pc_hal::traits::MemoryInterface,
{
    pub bar0: dev::Intel82559ES::Bar0::Mem<IM>,
    pub device: PD,
    pub dma_space: Dma,
}

pub struct InitializedDevice<E, IM, PD, D, Res, MM, Dma>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    PD: pc_hal::traits::PciDevice<Error = E, Device = D, Resource = Res, IoMem = IM>,
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
    IM: pc_hal::traits::MemoryInterface,
{
    pub bar0: dev::Intel82559ES::Bar0::Mem<IM>,
    pub device: PD,
    pub rx_queue: RefCell<RxQueue<E, Dma, MM>>,
    pub tx_queue: RefCell<TxQueue<E, Dma, MM>>,
    pub dma_space: Dma,
    pub pool: Mempool<E, Dma, MM>,
}

pub struct RxQueue<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub descriptors: DmaMemory<E, Dma, MM>,
    pub num_descriptors: u16,
    pub rx_index: usize,
    pub bufs_in_use: Vec<usize>,
}
pub struct TxQueue<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error = E, DmaSpace = Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub descriptors: DmaMemory<E, Dma, MM>,
    pub num_descriptors: u16,
    pub clean_index: usize,
    pub tx_index: usize,
    pub bufs_in_use: VecDeque<usize>,
}
/*
pub struct Interrupts<ISR> {
    pub(crate) timeout_ms: i16,
    pub(crate) itr_rate: u8,
    pub(crate) queues: Vec<InterruptsQueue<ISR>>,
}

pub struct InterruptsQueue<ISR> {
    pub(crate) isr: ISR,
    pub(crate) instr_counter: u64,
    pub(crate) last_time_checked: Instant,
    pub(crate) rx_pkts: u64,
    pub(crate) interval: u64,
    pub(crate) moving_avg: InterruptMovingAvg,
}
*/

#[derive(Default)]
pub struct InterruptMovingAvg {
    pub measured_rates: VecDeque<u64>,
    pub sum: u64,
}

#[derive(Default, Debug, Copy, Clone)]
pub struct DeviceStats {
    pub rx_pkts: u64,
    pub tx_pkts: u64,
    pub rx_bytes: u64,
    pub tx_bytes: u64,
}
