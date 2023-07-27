use std::rc::Rc;

use crate::{dev, dma::{DmaMemory, Mempool}};

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

pub struct Device<E, IM, PD, D, Res, Dma, MM>
where
    D: pc_hal::traits::Device,
    Res: pc_hal::traits::Resource,
    IM: pc_hal::traits::IoMem<Error=E>,
    PD: pc_hal::traits::PciDevice<Error=E, Device=D, Resource=Res> + AsMut<D>,
    MM: pc_hal::traits::MappableMemory<Error=E, DmaSpace=Dma>,
    Dma: pc_hal::traits::DmaSpace,
{
    pub (crate) bar0: dev::Intel82559ES::Bar0<IM>,
    pub (crate) device: PD,
    pub (crate) num_rx_queues: u16,
    pub (crate) num_tx_queues: u16,
    pub (crate) rx_queues: Vec<RxQueue<E, Dma, MM>>,
    pub (crate) tx_queues: Vec<TxQueue>,
    pub (crate) interrupts: Interrupts,
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
pub struct TxQueue {}
pub struct Interrupts {
    pub (crate) enabled : bool
}

/* Receive Descriptor - Advanced */
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ixgbe_adv_rx_desc_read {
    pub pkt_addr: u64,
    /* Packet buffer address */
    pub hdr_addr: u64,
    /* Header buffer address */
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ixgbe_adv_rx_desc_wb_lower_lo_dword_hs_rss {
    pub pkt_info: u16,
    /* RSS, Pkt type */
    pub hdr_info: u16,
    /* Splithdr, hdrlen */
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union ixgbe_adv_rx_desc_wb_lower_lo_dword {
    pub data: u32,
    pub hs_rss: ixgbe_adv_rx_desc_wb_lower_lo_dword_hs_rss,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ixgbe_adv_rx_desc_wb_lower_hi_dword_csum_ip {
    pub ip_id: u16,
    /* IP id */
    pub csum: u16,
    /* Packet Checksum */
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union ixgbe_adv_rx_desc_wb_lower_hi_dword {
    pub rss: u32,
    /* RSS Hash */
    pub csum_ip: ixgbe_adv_rx_desc_wb_lower_hi_dword_csum_ip,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct ixgbe_adv_rx_desc_wb_lower {
    pub lo_dword: ixgbe_adv_rx_desc_wb_lower_lo_dword,
    pub hi_dword: ixgbe_adv_rx_desc_wb_lower_hi_dword,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ixgbe_adv_rx_desc_wb_upper {
    pub status_error: u32,
    /* ext status/error */
    pub length: u16,
    /* Packet length */
    pub vlan: u16,
    /* VLAN tag */
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct ixgbe_adv_rx_desc_wb {
    pub lower: ixgbe_adv_rx_desc_wb_lower,
    pub upper: ixgbe_adv_rx_desc_wb_upper,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union ixgbe_adv_rx_desc {
    pub read: ixgbe_adv_rx_desc_read,
    pub wb: ixgbe_adv_rx_desc_wb, /* writeback */
    _union_align: [u64; 2],
}