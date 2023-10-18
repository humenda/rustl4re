#[cfg(kani)]
pub const NUM_RX_QUEUE_ENTRIES: u16 = 16;
#[cfg(kani)]
pub const NUM_TX_QUEUE_ENTRIES: u16 = 16;
#[cfg(kani)]
pub const BATCH_SIZE: usize = 1;

#[cfg(not(kani))]
pub const NUM_RX_QUEUE_ENTRIES: u16 = 512;
#[cfg(not(kani))]
pub const NUM_TX_QUEUE_ENTRIES: u16 = 512;
#[cfg(not(kani))]
pub const BATCH_SIZE: usize = 64;

pub const TX_CLEAN_BATCH: usize = 32;

pub const PKT_BUF_ENTRY_SIZE: usize = 2048;

pub const INTERRUPT_INITIAL_INTERVAL: u64 = 1_000_000_000;

pub const SRRCTL_DESCTYPE_ADV_ONE_BUFFER: u8 = 0b001;

pub const AUTOC_LMS_10G_SFI: u8 = 0b011;
pub const AUTOC2_10G_PMA_SERIAL_SFI: u8 = 0b10;

pub const LINKS_LINK_SPEED_100M: u8 = 0b01;
pub const LINKS_LINK_SPEED_1G: u8 = 0b10;
pub const LINKS_LINK_SPEED_10G: u8 = 0b11;

pub const ADV_TX_DESC_DTYP_DATA: u8 = 0b0011;

pub const WAIT_LIMIT: usize = 10;
