use std::cell::RefCell;
use std::rc::Rc;

use verix_lib::dev::VENDOR_ID;

use crate::constants::WriteRegister;
use crate::constants::mmio::{Eimc, IXGBE_EIMC, MmioState, IXGBE_CTRL, Ctrl, IXGBE_EEC, IXGBE_RAH0, IXGBE_RAL0, IXGBE_RDRXCTL, IXGBE_AUTOC, Autoc, IXGBE_AUTOC2, Autoc2, IXGBE_LINKS, IXGBE_GPRC, IXGBE_GPTC, IXGBE_GORCL, IXGBE_GORCH, IXGBE_GOTCL, IXGBE_GOTCH, IXGBE_RXCTRL, Rxctrl, IXGBE_RXPBSIZE_MIN, IXGBE_RXPBSIZE_MAX, IXGBE_RXPBSIZE_IDX, Rxpbsize, IXGBE_HLREG0, Hlreg0, Rdrxctl, IXGBE_FCTRL, Fctrl, IXGBE_SRCCTRL0, Srrctl, IXGBE_RDBAL0, IXGBE_RDBAH0, IXGBE_RDLEN0, DescriptorLen, IXGBE_RDH0, IXGBE_RDT0, QueuePointer, IXGBE_CTRL_EXT, CtrlExt, IXGBE_DCA_RXCTRL0, DcaRxctrl, IXGBE_MRQC, Mrqc, IXGBE_PFVTCTL, Pfvtctl, IXGBE_RTTDCS, Rttdcs, IXGBE_TXPBSIZE_MIN, IXGBE_TXPBSIZE_MAX, IXGBE_TXPBSIZE_IDX, Txpbsize, IXGBE_DTXMXSZRQ, Dtxmxszrq, IXGBE_TXPBTHRESH_MIN, IXGBE_TXPBTHRESH_MAX, IXGBE_TXPBTHRESH_IDX, Txpbthresh, IXGBE_TDBAL0, IXGBE_TDBAH0, IXGBE_TDLEN0, IXGBE_TXDCTL0, Txdctl, IXGBE_DMATXCTL, Dmatxctl, IXGBE_RXDCTL0, Rxdctl, IXGBE_TDT0, IXGBE_TDH0};
use crate::constants::pci::{
    Command, PciState, BAR0_ADDR, COMMAND_ADDR, DEVICE_ID, DEVICE_ID_ADDR, VENDOR_ID_ADDR, BAR_ALIGNMENT, BAR0_SIZE,
};
use crate::emulator::{BasicDevice, Device, Error, Resource, BasicResource, DmaDomainResource};

pub struct IxDevice {
    pub(crate) basic: BasicDevice,
    pub(crate) pci_state: PciState,
    pub(crate) dma_dom: Rc<DmaDomainResource>,
    bar_res: BasicResource,
    eec_read_limit: u8,
    dmaidone_read_limit: u8,
    links_read_limit: u8,
    rxdctl0_read_limit: u8,
    txdctl0_read_limit: u8,
    mac_addr: [u8; 6],
}

impl IxDevice {
    pub fn new(bar0_addr: u32, dma_domain_start: u32, eec_read_limit: u8, dmaidone_read_limit: u8, links_read_limit: u8, rxdctl0_read_limit: u8, txdctl0_read_limit: u8, mac_addr: [u8; 6]) -> Self {
        // 16 byte aligned
        assert!(bar0_addr % 16 == 0);

        let pci_state = PciState::new(bar0_addr);
        let ifaces = vec![pc_hal::traits::BusInterface::Pcidev];

        let dma_dom = Rc::new(DmaDomainResource {
            start: dma_domain_start as usize,
            end: (dma_domain_start as usize) + 1,
            mapping: RefCell::new(Vec::new())
        });

        let bar_res = BasicResource {
            start: bar0_addr as usize,
            end: bar0_addr as usize + BAR0_SIZE,
            typ: pc_hal::traits::ResourceType::Mem
        };
        // Resources don't overlap

        Self {
            basic: BasicDevice::new(ifaces),
            pci_state,
            eec_read_limit,
            dmaidone_read_limit,
            links_read_limit,
            rxdctl0_read_limit,
            txdctl0_read_limit,
            mac_addr,
            dma_dom,
            bar_res,
        }
    }
}

impl pc_hal::traits::Device for IxDevice {
    type Resource = <Device as pc_hal::traits::Device>::Resource;
    type ResourceIter<'a> = <Device as pc_hal::traits::Device>::ResourceIter<'a>;

    fn resource_iter<'a>(&'a self) -> Self::ResourceIter<'a> {
        vec![
            Resource::Basic(self.bar_res.clone()),
            Resource::DmaDomain(self.dma_dom.clone())
        ].into_iter()
    }

    fn supports_interface(&self, iface: pc_hal::traits::BusInterface) -> bool {
        self.basic.supports_interface(iface)
    }
}

impl pc_hal::traits::FailibleMemoryInterface32 for IxDevice {
    type Error = Error;
    type Addr = u32;

    fn write8(&mut self, offset: Self::Addr, val: u8) -> Result<(), Self::Error> {
        match offset {
            _ => panic!("Invalid pci write8 at: 0x{:x} = 0x{:x}", offset, val),
        }
    }

    fn write16(&mut self, offset: Self::Addr, val: u16) -> Result<(), Self::Error> {
        match offset {
            COMMAND_ADDR => {
                let proposed = Command(val);
                proposed.assert_valid();
                // Command has no effect on write, just take over
                self.pci_state.command = proposed;
            }
            _ => panic!("Invalid pci write16 at: 0x{:x}", offset),
        }
        Ok(())
    }

    fn write32(&mut self, offset: Self::Addr, val: u32) -> Result<(), Self::Error> {
        match offset {
            BAR0_ADDR => {
                if val == self.pci_state.bar0.orig_reg.0 {
                    // The value is reset after obtaining the length
                    self.pci_state.bar0.reg = self.pci_state.bar0.orig_reg.clone();
                } else {
                    // We expect to receive the length masking
                    assert!(val == 0xFFFFFFFF);
                    // memory BAR should be disabled while doing this
                    assert!(self.pci_state.command.mem_en() == false);

                    let encoded_len = !(self.pci_state.bar0.len as u32 - 1);
                    self.pci_state.bar0.reg.set_addr(encoded_len >> 4);
                }
            }
            _ => panic!("Invalid pci write32 at: 0x{:x}", offset),
        }
        Ok(())
    }

    fn read8(&self, offset: Self::Addr) -> Result<u8, Self::Error> {
        match offset {
            _ => panic!("Invalid pci read8 at: 0x{:x}", offset),
        }
    }

    fn read16(&self, offset: Self::Addr) -> Result<u16, Self::Error> {
        let res = match offset {
            VENDOR_ID_ADDR => VENDOR_ID,
            DEVICE_ID_ADDR => DEVICE_ID,
            COMMAND_ADDR => self.pci_state.command.0,
            _ => panic!("Invalid pci read16 at: 0x{:x}", offset),
        };
        Ok(res)
    }

    fn read32(&self, offset: Self::Addr) -> Result<u32, Self::Error> {
        let res = match offset {
            BAR0_ADDR => self.pci_state.bar0.reg.0,
            _ => panic!("Invalid pci read32 at: 0x{:x}", offset),
        };
        Ok(res)
    }
}

impl pc_hal::traits::PciDevice for IxDevice {
    type Device = Device;
    type IoMem = IoMem;

    fn try_of_device(dev: Self::Device) -> Option<Self> {
        match dev {
            Device::Ix(dev) => Some(dev),
            _ => panic!("Attempt to create IxDevice from bad device"),
        }
    }

    fn request_iomem(
        &mut self,
        phys: u64,
        size: u64,
        flags: pc_hal::traits::IoMemFlags,
    ) -> Result<Self::IoMem, Self::Error> {
        let phys: u32 = phys.try_into().unwrap();
        assert!(self.pci_state.bar0.reg.0 == self.pci_state.bar0.orig_reg.0);
        assert!(phys == self.pci_state.bar0.reg.addr() << BAR_ALIGNMENT);
        assert!(size == self.pci_state.bar0.len as u64);
        assert!(flags.contains(pc_hal::traits::IoMemFlags::NONCACHED));
        assert!(!flags.contains(pc_hal::traits::IoMemFlags::CACHED));
        Ok(IoMem::new(IoMemInner::new(self.eec_read_limit, self.dmaidone_read_limit, self.links_read_limit, self.rxdctl0_read_limit, self.txdctl0_read_limit, self.mac_addr)))
    }
}

#[derive(PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum InitializationState {
    DisableInterruptInitial = 0,
    Reset = 1,
    DisableInterruptAfterReset = 2,
    WaitEeprom = 3,
    WaitDma = 4,
    SetupLink = 5,
    WaitLink = 6,
    InitializeStatistics = 7,
    DisablingReceive = 8,
    ConfigReceive = 9,
    ConfigTransmitMac = 10,
    ConfigTransmitDcb = 11,
    ConfigTransmit = 12,
    RxStart = 13,
    WaitRxStart = 14,
    TxStart = 15,
    WaitTxStart = 16,
    Done = 17
}

pub struct WaitRegConfig {
    pub reads: u16,
    pub limit: u8
}

impl WaitRegConfig {
    fn new(limit: u8) -> WaitRegConfig {
        WaitRegConfig { reads: 0, limit }
    }
}

pub struct IoMem(RefCell<IoMemInner>);

impl IoMem {
    pub fn new(inner: IoMemInner) -> Self {
        Self(RefCell::new(inner))
    }
}

pub struct IoMemInner {
    pub init_state: InitializationState,
    mmio: MmioState,
    eec_reads: WaitRegConfig,
    dmaidone_reads: WaitRegConfig,
    links_reads: WaitRegConfig,
    rxdctl0_reads: WaitRegConfig,
    txdctl0_reads: WaitRegConfig,
    mac_addr: [u8; 6],
}

impl IoMemInner {
    pub fn new(eec_read_limit: u8, dmaidone_read_limit:u8, links_read_limit: u8, rxdctl0_read_limit: u8, txdctl0_read_limit: u8, mac_addr: [u8; 6]) -> IoMemInner {
        IoMemInner {
            init_state: InitializationState::DisableInterruptInitial,
            mmio: MmioState::new(mac_addr),
            eec_reads: WaitRegConfig::new(eec_read_limit),
            dmaidone_reads: WaitRegConfig::new(dmaidone_read_limit),
            links_reads: WaitRegConfig::new(links_read_limit),
            rxdctl0_reads: WaitRegConfig::new(rxdctl0_read_limit),
            txdctl0_reads: WaitRegConfig::new(txdctl0_read_limit),
            mac_addr
        }
    }

    fn check_stat_reset(&mut self) {
        let mmio = &self.mmio;
        if 
            mmio.gprc == 0 &&
            mmio.gptc == 0 &&
            mmio.gorcl == 0 &&
            mmio.gorch == 0 &&
            mmio.gotcl == 0 &&
            mmio.gotch == 0 &&
            self.init_state == InitializationState::InitializeStatistics
        {
            self.init_state = InitializationState::DisablingReceive;
        }
    }

    fn handle_write32(&mut self, offset: usize, val: u32) {
        match offset {
            IXGBE_EIMC => {
                let proposed = Eimc(val);
                proposed.assert_valid();
                // This is the only value that makes sense
                assert!(proposed.interrupt_mask() == u32::MAX >> 1);
                self.mmio.eimc = proposed;
                self.init_state = match self.init_state {
                    InitializationState::DisableInterruptInitial => InitializationState::Reset,
                    InitializationState::DisableInterruptAfterReset => InitializationState::WaitEeprom,
                    _ => panic!("shouldn't be necessary")
                }
            } ,
            IXGBE_CTRL => {
                assert!(self.init_state == InitializationState::Reset);
                let proposed = Ctrl(val);
                proposed.assert_valid();

                assert!(proposed.lrst() == true);
                assert!(proposed.rst() == true);

                // Reset our MMIO state
                self.mmio = MmioState::new(self.mac_addr);
                self.init_state = InitializationState::DisableInterruptAfterReset;
            },
            IXGBE_AUTOC => {
                assert!(self.init_state == InitializationState::SetupLink);
                let proposed = Autoc(val);
                proposed.assert_valid();
                self.mmio.autoc = proposed;

                if self.mmio.autoc.restart_an() {
                    /* 4.6.4.2 says to configure LMS and 10G_PARALLEL_PMA_PMD in AUTOC
                     * as well as 10G_PMA_PMD_Serial in AUTOC2.
                     */ 
                    // XAUI
                    assert!(self.mmio.autoc.teng_pma_pmd_parallel() == 0b00);
                    // SFI
                    assert!(self.mmio.autoc.lms() == 0b011);
                    // SFI
                    assert!(self.mmio.autoc2.teng_pam_pmd_serial() == 0b10);
                    self.init_state = InitializationState::WaitLink;
                }
            },
            IXGBE_AUTOC2 => {
                assert!(self.init_state == InitializationState::SetupLink);
                let proposed = Autoc2(val);
                proposed.assert_valid();
                self.mmio.autoc2 = proposed;
            }
            IXGBE_RXCTRL => {
                assert!(InitializationState::DisablingReceive <= self.init_state);
                let proposed = Rxctrl(val);
                proposed.assert_valid();
                self.mmio.rxctrl = proposed;
                if self.mmio.rxctrl.rxen() == false && InitializationState::DisablingReceive == self.init_state {
                    self.init_state = InitializationState::ConfigReceive;
                    return;
                } else if self.mmio.rxctrl.rxen() == true && InitializationState::ConfigReceive == self.init_state {
                    /* For a config to be valid we follow the definition of 4.6.7.
                     * First we are supposed to setup RAL, RAH, PFUTA, VFTA, PFVLVF, MPSAR and PFLVFB.
                     * Since we do not make use of VLAN and just want to use the default MAC we don't touch these -> no need for assertions.
                     * Since we are also not interested in filters for now we can also ignore these.
                     * We do use a single offloading feature, CRC stripping. THe datasheet remark that:
                     * "RDRXCTL.CRCStrip and HLREG0.RXCRCSTRP must be set to the same value"
                     */
                    assert!(self.mmio.rdrxctl.crcstrip() == self.mmio.hlreg0.rxcrcstrp()); // is equal
                    assert!(self.mmio.rdrxctl.crcstrip() == true); // is set
                    // Furthermore: At the same time the RDRXCTL.RSCFRSTSIZE should be set to 0x0 
                    assert!(self.mmio.rdrxctl.rscfrstsize() == 0);

                    assert!(self.mmio.fctrl.mpe() == true);
                    assert!(self.mmio.fctrl.upe() == true);
                    assert!(self.mmio.fctrl.bam() == true);

                    // Next setup the values from 4.6.11.3.4 since we are DCB off VT off
                    assert!(self.mmio.rxpbsizes[0].size() == 0x200);
                    for i in 1..8  {
                        assert!(self.mmio.rxpbsizes[i].size() == 0x0);
                    }
                    assert!(self.mmio.mrqc.mrqe() == 0x0); // we don't do RSS
                    assert!(self.mmio.mrqc.rss_field_en() == 0x0);
                    assert!(self.mmio.pfvtctl.vt_ena() == false);
                    assert!(self.mmio.rttup2tc.up0map() == 0);
                    assert!(self.mmio.rttup2tc.up1map() == 0);
                    assert!(self.mmio.rttup2tc.up2map() == 0);
                    assert!(self.mmio.rttup2tc.up3map() == 0);
                    assert!(self.mmio.rttup2tc.up4map() == 0);
                    assert!(self.mmio.rttup2tc.up5map() == 0);
                    assert!(self.mmio.rttup2tc.up6map() == 0);
                    assert!(self.mmio.rttup2tc.up7map() == 0);
                    // TODO: there is a bunch more stuff to assert here in principle, but it is all covered by default values anyways

                    // We don't want to do jumbo or linksec so we can ignore the jumbo field
                    // We are also not interested in receive coalescing for now.

                    // Per queue setup
                    // RDBAL and RDBAH should be a valid pointer and the region should be valid for RDLEN.
                    // Since asserting this is quite hard we simple probe the first and last byte.
                    let addr : *mut u8 = (((self.mmio.rdbah0 as u64) << 32) | (self.mmio.rdbal0 as u64)) as *mut u8;
                    let len = self.mmio.rdlen0.0;
                    // We know memory should be configured to 0xff
                    assert!(unsafe { addr.read() } == 0xff);
                    assert!(unsafe { addr.add(len as usize - 1).read() } == 0xff);
                    assert!(self.mmio.srrctl0.bsizepacket() == 0x2);
                    assert!(self.mmio.srrctl0.desctype() == 0b001);
                    assert!(self.mmio.srrctl0.drop_en() == true);
                    
                    // We do not care about header split so no PSRTYPE
                    // We do also not care about RSC so no RSCCTL
                    // We defer the queue start to after TX has been configured

                    assert!(self.mmio.dca_rxctrl0.reserved3() == false);
                    self.init_state = InitializationState::ConfigTransmitMac;
                } else {
                    panic!("Invalid write");
                }
            },
            IXGBE_HLREG0 => {
                assert!(InitializationState::ConfigReceive == self.init_state || InitializationState::ConfigTransmitMac == self.init_state);
                let proposed = Hlreg0(val);
                proposed.assert_valid();
                self.mmio.hlreg0 = proposed;
            }
            IXGBE_RDRXCTL => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                let proposed = Rdrxctl(val);
                proposed.assert_valid();
                self.mmio.rdrxctl = proposed;
            },
            IXGBE_FCTRL => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                let proposed = Fctrl(val);
                proposed.assert_valid();
                self.mmio.fctrl = proposed;
            },
            IXGBE_SRCCTRL0 => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                let proposed = Srrctl(val);
                proposed.assert_valid();
                self.mmio.srrctl0 = proposed;
            }
            IXGBE_RDBAL0 => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                self.mmio.rdbal0 = val;
            }
            IXGBE_RDBAH0 => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                self.mmio.rdbah0 = val;
            },
            IXGBE_RDLEN0 => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                let proposed = DescriptorLen(val);
                proposed.assert_valid();
                self.mmio.rdlen0 = proposed;
            }
            IXGBE_RDH0 => {
                assert!(InitializationState::ConfigReceive <= self.init_state);
                let proposed = QueuePointer(val);
                proposed.assert_valid();
                self.mmio.rdh0 = proposed;
            }
            IXGBE_RDT0 => {
                assert!(InitializationState::ConfigReceive <= self.init_state);
                let proposed = QueuePointer(val);
                proposed.assert_valid();
                self.mmio.rdt0 = proposed;
            }
            IXGBE_CTRL_EXT => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                let proposed = CtrlExt(val);
                proposed.assert_valid();
                self.mmio.ctrl_ext = proposed;
            }
            IXGBE_DCA_RXCTRL0 => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                let proposed = DcaRxctrl(val);
                proposed.assert_valid();
                self.mmio.dca_rxctrl0 = proposed;
            }
            IXGBE_MRQC => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                let proposed = Mrqc(val);
                proposed.assert_valid();
                self.mmio.mrqc = proposed;
            }
            IXGBE_PFVTCTL => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                let proposed = Pfvtctl(val);
                proposed.assert_valid();
                self.mmio.pfvtctl = proposed;
            }
            IXGBE_RTTDCS => {
                assert!(InitializationState::ConfigTransmitMac == self.init_state || InitializationState::ConfigTransmitDcb == self.init_state);
                let proposed = Rttdcs(val);
                proposed.assert_valid();
                self.mmio.rttdcs = proposed;
                if self.mmio.rttdcs.arbdis() && self.init_state == InitializationState::ConfigTransmitMac {
                    // according to 4.6.8
                    // We don't care about TCP segmentation for now so only the hlreg0 values matter
                    assert!(self.mmio.hlreg0.txcrcen() && self.mmio.hlreg0.txpaden());
                    self.init_state = InitializationState::ConfigTransmitDcb;
                } else if !self.mmio.rttdcs.arbdis() && self.init_state == InitializationState::ConfigTransmitDcb {
                    // According to 4.6.11.3.4
                    assert!(self.mmio.txpbsizes[0].size() == 0xa0);
                    assert!(self.mmio.txpbthresh[0].thresh() == 0xa0);
                    for i in 1..8  {
                        assert!(self.mmio.txpbsizes[i].size() == 0x0);
                        assert!(self.mmio.txpbthresh[i].thresh() == 0x0);
                    }
                    // the mtqc stuff is as initialized and unimplemented by us so no need to check, same with rttup2tc
                    assert!(self.mmio.dtxmxszrq.max_bytes_num_req() == 0xfff);
                    self.init_state = InitializationState::ConfigTransmit;
                } else {
                    panic!("Invalid rttdcs access");
                }
            }
            IXGBE_DTXMXSZRQ => {
                assert!(InitializationState::ConfigTransmitDcb == self.init_state);
                let proposed = Dtxmxszrq(val);
                proposed.assert_valid();
                self.mmio.dtxmxszrq = proposed;
            }
            IXGBE_TDBAL0 => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                self.mmio.tdbal0 = val;
            }
            IXGBE_TDBAH0 => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                self.mmio.tdbah0 = val;
            }
            IXGBE_TDLEN0 => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                let proposed = DescriptorLen(val);
                proposed.assert_valid();
                self.mmio.tdlen0 = proposed;
            }
            IXGBE_TXDCTL0 => {
                assert!(InitializationState::ConfigTransmit <= self.init_state);
                let mut proposed = Txdctl(val);
                proposed.assert_valid();
                // TODO: 7.2.3.4.1 tells us how to compute this, use it instead of constants
                let pthresh = 36;
                let hthresh = 8;
                let wthresh = 4;
                assert!(proposed.pthresh() == pthresh);
                assert!(proposed.hthresh() == hthresh);
                assert!(proposed.wthresh() == wthresh);

                // for now we don't wish to enable the queue yet
                if self.init_state == InitializationState::TxStart && proposed.enable() {
                    self.init_state = InitializationState::WaitTxStart;
                    proposed.set_enable(false);
                } else {
                    assert!(proposed.enable() == false);
                }
                self.mmio.txdctl0 = proposed;
            }
            IXGBE_DMATXCTL => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                let proposed = Dmatxctl(val);
                proposed.assert_valid();
                if proposed.te() {
                    self.init_state = InitializationState::RxStart;
                }
            }
            IXGBE_RXDCTL0 => {
                assert!(InitializationState::RxStart <= self.init_state);
                let mut proposed = Rxdctl(val);
                proposed.assert_valid();
                if proposed.enable() {
                    self.init_state = InitializationState::WaitRxStart;
                    proposed.set_enable(false);
                }
                self.mmio.rxdctl0 = proposed;
            }
            IXGBE_TDH0 => {
                assert!(InitializationState::TxStart <= self.init_state);
                let proposed = QueuePointer(val);
                proposed.assert_valid();
                self.mmio.tdh0 = proposed;
            }
            IXGBE_TDT0 => {
                assert!(InitializationState::TxStart <= self.init_state);
                let proposed = QueuePointer(val);
                proposed.assert_valid();
                self.mmio.tdt0 = proposed;
            }
            IXGBE_RXPBSIZE_MIN..=IXGBE_RXPBSIZE_MAX => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                let proposed = Rxpbsize(val);
                proposed.assert_valid();
                let idx = IXGBE_RXPBSIZE_IDX(offset);
                self.mmio.rxpbsizes[idx] = proposed;
            },
            IXGBE_TXPBSIZE_MIN..=IXGBE_TXPBSIZE_MAX => {
                assert!(InitializationState::ConfigTransmitDcb == self.init_state);
                let proposed = Txpbsize(val);
                proposed.assert_valid();
                let idx = IXGBE_TXPBSIZE_IDX(offset);
                self.mmio.txpbsizes[idx] = proposed;
            },
            IXGBE_TXPBTHRESH_MIN..=IXGBE_TXPBTHRESH_MAX => {
                assert!(InitializationState::ConfigTransmitDcb == self.init_state);
                let proposed = Txpbthresh(val);
                proposed.assert_valid();
                let idx = IXGBE_TXPBTHRESH_IDX(offset);
                self.mmio.txpbthresh[idx] = proposed;
            },
            _ => panic!("invalid write offset: 0x{:x}", offset)
        }
    }

    fn handle_read32(&mut self, offset: usize) -> u32 {
        match offset {
            IXGBE_CTRL => self.mmio.ctrl.0,
            IXGBE_EEC => {
                // It only makes sense to read this reg in this state
                assert!(self.init_state == InitializationState::WaitEeprom);
                if self.eec_reads.reads > self.eec_reads.limit.into() {
                    self.mmio.eec.set_auto_rd(true);
                    self.init_state = InitializationState::WaitDma;
                } else {
                    self.eec_reads.reads += 1;
                }
                self.mmio.eec.0
            },
            IXGBE_RAL0 => {
                assert!(InitializationState::WaitEeprom < self.init_state);
                self.mmio.ral.0
            },
            IXGBE_RAH0 => {
                assert!(InitializationState::WaitEeprom < self.init_state);
                self.mmio.rah.0
            },
            IXGBE_RDRXCTL => {
                assert!(InitializationState::WaitDma <= self.init_state);
                if self.init_state == InitializationState::WaitDma {
                    if self.dmaidone_reads.reads > self.dmaidone_reads.limit.into() {
                        self.mmio.rdrxctl.set_dmaidone(true);
                        self.init_state = InitializationState::SetupLink;
                    } else {
                        self.dmaidone_reads.reads += 1;
                    }
                }
                self.mmio.rdrxctl.0
            },
            // TODO: This should use the semaphore mechanism described in the datasheet
            IXGBE_AUTOC => {
                assert!(self.init_state == InitializationState::SetupLink);
                self.mmio.autoc.0
            },
            IXGBE_AUTOC2 => {
                assert!(self.init_state == InitializationState::SetupLink);
                self.mmio.autoc2.0
            },
            IXGBE_LINKS => {
                assert!(InitializationState::WaitLink <= self.init_state);
                if self.links_reads.reads > self.links_reads.limit.into() {
                    // TODO
                    self.mmio.links.set_link_status(true);
                    self.mmio.links.set_teng_ser_en(true);
                    self.mmio.links.set_mlink_mode(0b10);
                    self.mmio.links.set_link_speed(0b11);
                    self.mmio.links.set_link_up(true);
                    self.init_state = InitializationState::InitializeStatistics;
                } else {
                    self.links_reads.reads += 1;
                }
                self.mmio.links.0
            },
            IXGBE_GPRC => {
                assert!(InitializationState::InitializeStatistics <= self.init_state);
                if self.init_state == InitializationState::InitializeStatistics {
                    self.mmio.gprc = 0;
                    self.check_stat_reset();
                }
                self.mmio.gprc
            }
            IXGBE_GPTC => {
                assert!(InitializationState::InitializeStatistics <= self.init_state);
                if self.init_state == InitializationState::InitializeStatistics {
                    self.mmio.gptc = 0;
                    self.check_stat_reset();
                }
                self.mmio.gptc
            }
            IXGBE_GORCL => {
                assert!(InitializationState::InitializeStatistics <= self.init_state);
                if self.init_state == InitializationState::InitializeStatistics {
                    self.mmio.gorcl = 0;
                    self.check_stat_reset();
                }
                self.mmio.gorcl
            }
            IXGBE_GORCH => {
                assert!(InitializationState::InitializeStatistics <= self.init_state);
                if self.init_state == InitializationState::InitializeStatistics {
                    self.mmio.gorch = 0;
                    self.check_stat_reset();
                }
                self.mmio.gorch.into()
            }
            IXGBE_GOTCL => {
                assert!(InitializationState::InitializeStatistics <= self.init_state);
                if self.init_state == InitializationState::InitializeStatistics {
                    self.mmio.gotcl = 0;
                    self.check_stat_reset();
                }
                self.mmio.gotcl
            }
            IXGBE_GOTCH => {
                assert!(InitializationState::InitializeStatistics <= self.init_state);
                if self.init_state == InitializationState::InitializeStatistics {
                    self.mmio.gotch = 0;
                    self.check_stat_reset();
                }
                self.mmio.gotch.into()
            }
            IXGBE_RXCTRL => {
                assert!(InitializationState::DisablingReceive <= self.init_state);
                self.mmio.rxctrl.0
            }
            IXGBE_HLREG0 => {
                assert!(InitializationState::ConfigReceive == self.init_state || InitializationState::ConfigTransmitMac == self.init_state);
                self.mmio.hlreg0.0
            }
            IXGBE_FCTRL => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                self.mmio.fctrl.0
            }
            IXGBE_SRCCTRL0 => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                self.mmio.srrctl0.0 
            }
            IXGBE_RDBAL0 => {
                assert!(InitializationState::ConfigReceive <= self.init_state);
                self.mmio.rdbal0
            }
            IXGBE_RDBAH0 => {
                assert!(InitializationState::ConfigReceive <= self.init_state);
                self.mmio.rdbah0
            }
            IXGBE_RDLEN0 => {
                assert!(InitializationState::ConfigReceive <= self.init_state);
                self.mmio.rdlen0.0
            }
            IXGBE_RDT0 => {
                assert!(InitializationState::ConfigReceive <= self.init_state );
                self.mmio.rdt0.0
            }
            IXGBE_RDH0 => {
                assert!(InitializationState::ConfigReceive <= self.init_state);
                self.mmio.rdh0.0
            }
            IXGBE_CTRL_EXT => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                self.mmio.ctrl_ext.0
            }
            IXGBE_DCA_RXCTRL0 => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                self.mmio.dca_rxctrl0.0
            }
            IXGBE_MRQC => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                self.mmio.mrqc.0 
            }
            IXGBE_PFVTCTL => {
                assert!(self.init_state == InitializationState::ConfigReceive);
                self.mmio.pfvtctl.0 
            }
            IXGBE_RTTDCS => {
                assert!(InitializationState::ConfigTransmitMac == self.init_state || InitializationState::ConfigTransmitDcb == self.init_state);
                self.mmio.rttdcs.0
            }
            IXGBE_DTXMXSZRQ => {
                assert!(InitializationState::ConfigTransmitDcb == self.init_state);
                self.mmio.dtxmxszrq.0
            }
            IXGBE_TDBAL0 => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                self.mmio.tdbal0
            }
            IXGBE_TDBAH0 => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                self.mmio.tdbah0
            }
            IXGBE_TDLEN0 => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                self.mmio.tdlen0.0
            }
            IXGBE_TXDCTL0 => {
                assert!(InitializationState::ConfigTransmit <= self.init_state);
                if self.init_state == InitializationState::WaitTxStart {
                    if self.txdctl0_reads.reads > self.txdctl0_reads.limit.into() {
                        // Here we assert that both the RX and the TX queue are configured correctly
                        assert!(self.mmio.rdh0.pos() == 0);
                        //assert!(self.mmio.rdt0.pos() == 511);
                        assert!(self.mmio.tdh0.pos() == 0);
                        assert!(self.mmio.tdt0.pos() == 0);

                        self.mmio.txdctl0.set_enable(true);
                        self.init_state = InitializationState::Done;
                    } else {
                        self.txdctl0_reads.reads += 1;
                    }
                }
                self.mmio.txdctl0.0
            }
            IXGBE_DMATXCTL => {
                assert!(InitializationState::ConfigTransmit == self.init_state);
                self.mmio.dmatxctl.0
            }
            IXGBE_RXDCTL0 => {
                assert!(InitializationState::RxStart <= self.init_state);
                if self.init_state == InitializationState::WaitRxStart {
                    if self.rxdctl0_reads.reads > self.rxdctl0_reads.limit.into() {
                        self.mmio.rxdctl0.set_enable(true);
                        self.init_state = InitializationState::TxStart;
                    } else {
                        self.rxdctl0_reads.reads += 1;
                    }
                }
                self.mmio.rxdctl0.0
            }
            IXGBE_TDT0 => {
                assert!(InitializationState::TxStart <= self.init_state );
                self.mmio.tdt0.0
            }
            IXGBE_TDH0 => {
                assert!(InitializationState::TxStart <= self.init_state);
                self.mmio.tdh0.0
            }
            IXGBE_RXPBSIZE_MIN..=IXGBE_RXPBSIZE_MAX => {
                assert!(InitializationState::ConfigReceive == self.init_state);
                let idx = IXGBE_RXPBSIZE_IDX(offset);
                self.mmio.rxpbsizes[idx].0
            },
            IXGBE_TXPBSIZE_MIN..=IXGBE_TXPBSIZE_MAX => {
                assert!(InitializationState::ConfigTransmitDcb == self.init_state);
                let idx = IXGBE_TXPBSIZE_IDX(offset);
                self.mmio.txpbsizes[idx].0
            },
            IXGBE_TXPBTHRESH_MIN..=IXGBE_TXPBTHRESH_MAX => {
                assert!(InitializationState::ConfigTransmitDcb == self.init_state);
                let idx = IXGBE_TXPBTHRESH_IDX(offset);
                self.mmio.txpbthresh[idx].0
            },
            _ => panic!("invalid read offset: 0x{:x}", offset),
        }
    }
}

impl pc_hal::traits::MemoryInterface for IoMem {
    unsafe fn write32(&self, offset: usize, val: u32) { self.0.borrow_mut().handle_write32(offset, val) }
    unsafe fn read32(&self, offset: usize) -> u32 { self.0.borrow_mut().handle_read32(offset) }

    unsafe fn write8(&self, _offset: usize, _val: u8) { panic!("only 32 bit"); }
    unsafe fn write16(&self, _offset: usize, _val: u16) { panic!("only 32 bit"); }
    unsafe fn write64(&self, _offset: usize, _val: u64) { panic!("only 32 bit"); }

    unsafe fn read8(&self, _offset: usize) -> u8 { panic!("only 32 bit"); }
    unsafe fn read16(&self, _offset: usize) -> u16 { panic!("only 32 bit"); }
    unsafe fn read64(&self, _offset: usize) -> u64 { panic!("only 32 bit"); }
}
