use std::{rc::Rc, time::Instant, collections::VecDeque};

use crate::{dev, dma::{DmaMemory, Mempool}};

use pc_hal_util::mmio::MsixDev;

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
    MempoolEmpty
}

impl<E> From<E> for Error<E> {
    fn from(value: E) -> Self {
        Self::HalError(value)
    }
}

pub type Result<T, E> = std::result::Result<T, Error<E>>;

pub struct Device<E, IM, PD, D, Res, Dma, MM, ISR>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    IM: pc_hal::traits::IoMem<Error=E>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>,
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub (crate) bar0: dev::Intel82559ES::Bar0::Mem<IM>,
    pub (crate) msix: MsixDev::Msix::Mem<IM>,
    pub (crate) device: PD,
    pub (crate) num_rx_queues: u16,
    pub (crate) num_tx_queues: u16,
    pub (crate) rx_queues: Vec<RxQueue<E, Dma, MM>>,
    pub (crate) tx_queues: Vec<TxQueue<E, Dma, MM>>,
    pub (crate) interrupts: Interrupts<ISR>,
    pub (crate) dma_space: Dma
}

pub struct RxQueue<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub (crate) descriptors: DmaMemory<E, Dma, MM>,
    pub (crate) pool: Rc<Mempool<E, Dma, MM>>,
    pub (crate) num_descriptors: usize,
    pub (crate) rx_index: usize,
    pub (crate) bufs_in_use: Vec<usize>
}
pub struct TxQueue<E, Dma, MM>
where
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub (crate) descriptors: DmaMemory<E, Dma, MM>,
    pub (crate) pool: Rc<Mempool<E, Dma, MM>>,
    pub (crate) num_descriptors: usize,
    pub (crate) clean_index: usize,
    pub (crate) tx_index: usize,
    pub (crate) bufs_in_use: Vec<usize>
}
pub struct Interrupts<ISR> {
    pub (crate) timeout_ms : i16,
    pub (crate) itr_rate : u32,
    pub (crate) queues : Vec<InterruptsQueue<ISR>>,
}

pub struct InterruptsQueue<ISR> {
    pub (crate) isr: ISR,
    pub (crate) instr_counter: u64,
    pub (crate) last_time_checked: Instant,
    pub (crate) rx_pkts: u64,
    pub (crate) interval: u64,
    pub (crate) moving_avg: InterruptMovingAvg,
}

#[derive(Default)]
pub struct InterruptMovingAvg {
    pub measured_rates: VecDeque<u64>,
    pub sum: u64
}

/* Transmit Descriptor - Advanced */
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ixgbe_adv_tx_desc_read {
    pub buffer_addr: u64,
    /* Address of descriptor's data buf */
    pub cmd_type_len: u32,
    pub olinfo_status: u32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ixgbe_adv_tx_desc_wb {
    pub rsvd: u64,
    /* Reserved */
    pub nxtseq_seed: u32,
    pub status: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union ixgbe_adv_tx_desc {
    pub read: ixgbe_adv_tx_desc_read,
    pub wb: ixgbe_adv_tx_desc_wb,
    _union_align: [u64; 2],
}