pub trait WriteRegister {
    fn assert_valid(&self);
}

pub mod pci {
    use bitfield::bitfield;

    use super::WriteRegister;

    pub const VENDOR_ID_ADDR: u32 = 0x0;
    pub const DEVICE_ID_ADDR: u32 = 0x2;
    pub const COMMAND_ADDR: u32 = 0x4;
    pub const BAR0_ADDR: u32 = 0x10;

    pub const VENDOR_ID: u16 = 0x8086;
    pub const DEVICE_ID: u16 = 0x10FB;
    pub const BAR0_SIZE: usize = 0x80000;
    pub const BAR_ALIGNMENT: usize = 4;

    pub struct PciState {
        pub command: Command,
        pub bar0: MemBar,
    }

    impl PciState {
        pub fn new(bar0_addr: u32) -> Self {
            Self {
                command: Default::default(),
                bar0: MemBar::new(bar0_addr, BAR0_SIZE),
            }
        }
    }

    pub struct MemBar {
        pub len: usize,
        pub orig_reg: MemBarReg,
        pub reg: MemBarReg,
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct MemBarReg(u32);
        pub reserved0, _: 0;
        pub typ, _: 2, 1;
        pub prefetchable, _: 3;
        pub addr, set_addr: 31, 4;
    }

    impl MemBar {
        fn new(addr: u32, len: usize) -> Self {
            let mut reg = MemBarReg(0);
            reg.set_addr(addr >> BAR_ALIGNMENT);

            Self {
                len,
                orig_reg: reg.clone(),
                reg,
            }
        }
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Command(u16);
        pub io_en, set_io_en: 0;
        pub mem_en, set_mem_en: 1;
        pub bme, set_bme: 2;
        pub special_cycle, _: 3;
        pub mwi_en, _: 4;
        pub palette_en, _: 5;
        pub parity_error, _: 6;
        pub wait_en, _: 7;
        pub serr_en, _: 8;
        pub fast_en, _: 9;
        pub interrupt_dis, set_interrupt_dis: 10;
        pub reserved0, _: 15, 11;
    }

    impl Default for Command {
        fn default() -> Self {
            let mut val = Command(0);
            val.set_interrupt_dis(true);
            val
        }
    }

    impl WriteRegister for Command {
        fn assert_valid(&self) {
            // Reserved fields shall stay at their default values
            assert!(self.special_cycle() == false);
            assert!(self.mwi_en() == false);
            assert!(self.palette_en() == false);
            assert!(self.wait_en() == false);
            assert!(self.fast_en() == false);
            assert!(self.reserved0() == 0);

            // Not implemented fields shall stay at their default values
            assert!(self.io_en() == false);
            assert!(self.parity_error() == false);
            assert!(self.serr_en() == false);
            assert!(self.interrupt_dis() == true);
        }
    }
}

pub mod mmio {
    use bitfield::bitfield;

    use super::WriteRegister;

    pub struct MmioState {
        pub eimc : Eimc,
        pub ctrl: Ctrl,
        pub eec: Eec,
        pub ral: Ral,
        pub rah: Rah,
        pub rdrxctl: Rdrxctl,
        pub autoc: Autoc,
        pub autoc2: Autoc2,
        pub links: Links,
        pub gprc: u32,
        pub gptc: u32,
        pub gorcl: u32,
        pub gorch: u8,
        pub gotcl: u32,
        pub gotch: u8,
        pub rxctrl: Rxctrl,
        pub rxpbsizes: [Rxpbsize; 8],
        pub hlreg0: Hlreg0,
        pub fctrl: Fctrl,
        pub srrctl0: Srrctl,
        pub rdbal0: u32,
        pub rdbah0: u32,
        pub rdlen0: DescriptorLen,
        pub rdt0: QueuePointer,
        pub rdh0: QueuePointer,
        pub ctrl_ext: CtrlExt,
        pub dca_rxctrl0: DcaRxctrl,
        pub mrqc: Mrqc,
        pub pfvtctl: Pfvtctl,
        pub rttup2tc: Rttup2tc,
        pub rttdcs: Rttdcs,
        pub txpbsizes: [Txpbsize; 8],
        pub dtxmxszrq: Dtxmxszrq,
        pub txpbthresh: [Txpbthresh; 8],
        pub tdbal0: u32,
        pub tdbah0: u32,
        pub tdlen0: DescriptorLen,
        pub txdctl0: Txdctl,
        pub dmatxctl : Dmatxctl,
        pub tdh0: QueuePointer,
        pub tdt0: QueuePointer,
        pub rxdctl0: Rxdctl,
    }

    impl MmioState {
        pub fn new(mac_addr: [u8; 6]) -> MmioState {
            let ral = Ral(u32::from_be_bytes([
                mac_addr[3],
                mac_addr[2],
                mac_addr[1],
                mac_addr[0]
            ]));
            let rah = Rah(u32::from_be_bytes([
                0,
                0,
                mac_addr[5],
                mac_addr[4]
            ]));

            let mut txpbthresh0 = Txpbthresh(0);
            txpbthresh0.set_thresh(0x96);
            let txpbthresh = [
                txpbthresh0,
                Txpbthresh(0),
                Txpbthresh(0),
                Txpbthresh(0),
                Txpbthresh(0),
                Txpbthresh(0),
                Txpbthresh(0),
                Txpbthresh(0),
            ];

            MmioState {
                eimc: Default::default(),
                ctrl: Default::default(),
                eec: Default::default(),
                ral,
                rah,
                rdrxctl: Default::default(),
                autoc: Default::default(),
                autoc2: Default::default(),
                links: Default::default(),
                gprc: 1,
                gptc: 1,
                gorcl: 1,
                gorch: 1,
                gotcl: 1,
                gotch: 1,
                rxctrl: Default::default(),
                rxpbsizes: Default::default(),
                hlreg0: Default::default(),
                fctrl: Default::default(),
                srrctl0: Default::default(),
                rdbal0: 0,
                rdbah0: 0,
                rdlen0: Default::default(),
                rdt0: Default::default(),
                rdh0: Default::default(),
                ctrl_ext: Default::default(),
                dca_rxctrl0: Default::default(),
                mrqc: Default::default(),
                pfvtctl: Default::default(),
                rttup2tc: Default::default(),
                rttdcs: Default::default(),
                txpbsizes: Default::default(),
                dtxmxszrq: Default::default(),
                txpbthresh,
                tdbal0: 0,
                tdbah0: 0,
                tdlen0: Default::default(),
                txdctl0: Default::default(),
                dmatxctl: Default::default(),
                tdh0: Default::default(),
                tdt0: Default::default(),
                rxdctl0: Default::default(),
            }
        }
    }

    pub const IXGBE_EIMC: usize = 0x00888;
    pub const IXGBE_CTRL: usize = 0x00000;
    pub const IXGBE_EEC: usize = 0x10010;
    pub const IXGBE_RAL0: usize = 0x0A200;
    pub const IXGBE_RAH0: usize = 0x0A204;
    pub const IXGBE_RDRXCTL: usize = 0x02F00;
    pub const IXGBE_AUTOC: usize = 0x042A0;
    pub const IXGBE_AUTOC2: usize = 0x042A8;
    pub const IXGBE_LINKS: usize = 0x042A4;
    pub const IXGBE_GPRC: usize = 0x04074;
    pub const IXGBE_GPTC: usize = 0x04080;
    pub const IXGBE_GORCL: usize = 0x04088;
    pub const IXGBE_GORCH: usize = 0x0408C;
    pub const IXGBE_GOTCL: usize = 0x04090;
    pub const IXGBE_GOTCH: usize = 0x04094;
    pub const IXGBE_RXCTRL: usize = 0x03000;
    pub const IXGBE_RXPBSIZE_BASE: usize = 0x03C00;
    pub const fn IXGBE_RXPBSIZE(i: usize) -> usize { assert!(i <= 7); IXGBE_RXPBSIZE_BASE + i * 4 }
    pub const fn IXGBE_RXPBSIZE_IDX(addr: usize) -> usize { (addr - IXGBE_RXPBSIZE_BASE) / 4 }
    pub const IXGBE_RXPBSIZE_MIN : usize = IXGBE_RXPBSIZE(0);
    pub const IXGBE_RXPBSIZE_MAX : usize = IXGBE_RXPBSIZE(7);
    pub const IXGBE_HLREG0 : usize = 0x04240;
    pub const IXGBE_FCTRL : usize = 0x05080;
    pub const IXGBE_SRCCTRL0 : usize = 0x2100;
    pub const IXGBE_RDBAL0 : usize = 0x01000;
    pub const IXGBE_RDBAH0 : usize = 0x01004;
    pub const IXGBE_RDLEN0 : usize = 0x01008;
    pub const IXGBE_RDH0 : usize = 0x01010;
    pub const IXGBE_RDT0 : usize = 0x01018;
    pub const IXGBE_CTRL_EXT : usize = 0x00018;
    pub const IXGBE_DCA_RXCTRL0 : usize = 0x02200;
    pub const IXGBE_MRQC : usize = 0x05818;
    pub const IXGBE_PFVTCTL : usize = 0x051B0;
    pub const IXGBE_RTTDCS : usize = 0x04900;
    pub const IXGBE_TXPBSIZE_BASE : usize = 0x0CC00;
    pub const fn IXGBE_TXPBSIZE(i: usize) -> usize { assert!(i <= 7); IXGBE_TXPBSIZE_BASE + i * 4 }
    pub const fn IXGBE_TXPBSIZE_IDX(addr: usize) -> usize { (addr - IXGBE_TXPBSIZE_BASE) / 4 }
    pub const IXGBE_TXPBSIZE_MIN : usize = IXGBE_TXPBSIZE(0);
    pub const IXGBE_TXPBSIZE_MAX : usize = IXGBE_TXPBSIZE(7);
    pub const IXGBE_DTXMXSZRQ : usize = 0x08100;
    pub const IXGBE_TXPBTHRESH_BASE : usize = 0x04950;
    pub const fn IXGBE_TXPBTHRESH(i: usize) -> usize { assert!(i <= 7); IXGBE_TXPBTHRESH_BASE + i * 4 }
    pub const fn IXGBE_TXPBTHRESH_IDX(addr: usize) -> usize { (addr - IXGBE_TXPBTHRESH_BASE) / 4 }
    pub const IXGBE_TXPBTHRESH_MIN : usize = IXGBE_TXPBTHRESH(0);
    pub const IXGBE_TXPBTHRESH_MAX : usize = IXGBE_TXPBTHRESH(7);
    pub const IXGBE_TDBAL0 : usize = 0x06000;
    pub const IXGBE_TDBAH0 : usize = 0x06004;
    pub const IXGBE_TDLEN0 : usize = 0x06008;
    pub const IXGBE_TXDCTL0 : usize = 0x06028;
    pub const IXGBE_DMATXCTL : usize = 0x04A80; 
    pub const IXGBE_RXDCTL0 : usize = 0x01028;
    pub const IXGBE_TDH0 : usize = 0x06010;
    pub const IXGBE_TDT0 : usize = 0x06018;

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Eimc(u32);
        pub interrupt_mask, set_interrupt_mask: 30, 0;
        pub reserved0, _: 31;
    }

    impl Default for Eimc {
        fn default() -> Self {
            Eimc(0)
        }
    }

    impl WriteRegister for Eimc {
        fn assert_valid(&self) {
            // Reserved fields shall stay at their default values
            assert!(self.reserved0() == false);
        }
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Ctrl(u32);
        pub reserved0, _: 1, 0;
        pub pcie_master_disable, _: 2;
        pub lrst, set_lrst: 3;
        pub reserved1, _: 25, 4;
        pub rst, set_rst: 26;
        pub reserved2, _: 31, 27;
    }

    impl Default for Ctrl {
        fn default() -> Self {
            Ctrl(0)
        }
    }

    impl WriteRegister for Ctrl {
        fn assert_valid(&self) {
            // Reserved fields shall stay at their default values
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == 0);
            assert!(self.reserved2() == 0);

            // In our driver there is no reaosn to set this bit
            assert!(self.pcie_master_disable() == false);
        }
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Eec(u32);
        pub ee_sk, _: 0;
        pub ee_cs, _: 1;
        pub ee_di, _: 2;
        pub ee_do, _: 3;
        pub fwe, set_fwe: 5, 4;
        pub ee_rq, _: 6;
        pub ee_gnt, _: 7;
        pub pres, _: 8;
        pub auto_rd, set_auto_rd: 9;
        pub reserved0, set_reserved0: 10;
        pub ee_size, set_ee_size: 14, 11;
        pub pci_ana_done, _: 15;
        pub pci_core_done, _: 16;
        pub pci_genarl_done, _: 17;
        pub pci_func_done, _: 18;
        pub core_done, _: 19;
        pub pci_core_csr_done, _: 20;
        pub mac_done, _: 21;
        pub reserved1, _: 31,22;
    }

    impl Default for Eec {
        fn default() -> Self {
            let mut val = Eec(0);
            val.set_fwe(0b01);
            val.set_reserved0(true);
            val.set_ee_size(0b0010);
            val
        }
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Ral(u32);
        pub ral, _: 31,0;
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Rah(u32);
        pub rah, _: 15,0;
        pub reserved0, _: 31,16;
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Rdrxctl(u32);
        pub reserved0, _: 0;
        pub crcstrip, _: 1;
        pub reserved1, _: 2;
        pub dmaidone, set_dmaidone: 3;
        pub reserved2, set_reserved2: 16, 4;
        pub rscfrstsize, set_rscfrstsize: 21, 17;
        pub reserved3, _: 24, 22;
        pub rscackc, _: 25;
        pub fcoe_wrfix, _: 26;
        pub reserved4, _: 31,27;
    }

    impl Default for Rdrxctl {
        fn default() -> Self {
            let mut val = Rdrxctl(0);
            val.set_reserved2(0x0880);
            val.set_rscfrstsize(0x8);
            val
        }
    }

    impl WriteRegister for Rdrxctl {
        fn assert_valid(&self) {
            assert!(self.reserved2() == 0x0880);
        }
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Autoc(u32);
        pub flu, _: 0;
        pub anack2, _: 1;
        pub ansf, set_ansf: 6, 2;
        pub teng_pma_pmd_parallel, set_teng_pma_pmd_parallel: 8, 7;
        pub oneg_pma_pmd, set_oneg_pma_pmd: 9;
        pub d10gmp, _: 10;
        pub ratd, _: 11;
        pub restart_an, _: 12;
        pub lms, set_lms: 15, 13;
        pub kr_suppport, set_kr_support: 16;
        pub fecr, _: 17;
        pub feca, set_feca: 18;
        pub anrxat, set_anrxat: 22, 19;
        pub anrxdm, set_anrxdm: 23;
        pub anrxlm, set_anrxlm: 24;
        pub anpdt, _: 26, 25;
        pub rf, _: 27;
        pub pb, _: 29, 28;
        pub kx_support, set_kx_support: 31, 30;
    }

    impl Default for Autoc {
        fn default() -> Autoc {
            let mut val = Autoc(0);
            val.set_ansf(0b00001);
            val.set_teng_pma_pmd_parallel(0b01);
            val.set_oneg_pma_pmd(true);
            val.set_lms(0b100);
            val.set_kr_support(true);
            val.set_feca(true);
            val.set_anrxat(0b0011);
            val.set_anrxdm(true);
            val.set_anrxlm(true);
            val.set_kx_support(0b11);
            val
        }
    }

    impl WriteRegister for Autoc {
        fn assert_valid(&self) {
            // All unimplemented fields are to stay at their default value
            assert!(self.ansf() == 0b00001);
            assert!(self.kr_suppport() == true);
            assert!(self.feca() == true);
            assert!(self.anrxat() == 0b0011);
            assert!(self.anrxdm() == true);
            assert!(self.anrxlm() == true);
            assert!(self.kx_support() == 0b11);
        }
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Autoc2(u32);
        pub reserved0, _: 15, 0;
        pub teng_pam_pmd_serial, _: 17, 16;
        pub ddpt, _: 18;
        pub reserved1, _: 27, 19;
        pub fasm, _: 28;
        pub reserved2, _: 29;
        pub pdd, _: 30;
        pub reserved3, _: 31;
    }

    impl Default for Autoc2 {
        fn default() -> Autoc2 {
            Autoc2(0) 
        }
    }

    impl WriteRegister for Autoc2 {
        fn assert_valid(&self) {
            // All reserved or unimplement fields are to stay at their default value
            assert!(self.reserved0() == 0);
            assert!(self.ddpt() == false);
            assert!(self.reserved1() == 0);
            assert!(self.fasm() == false);
            assert!(self.reserved2() == false);
            assert!(self.pdd() == false);
            assert!(self.reserved3() == false);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Links(u32);
        pub kx_sig_det, _: 0;
        pub fec_sig_det, _: 1;
        pub fec_block_lock, _: 2;
        pub kr_hi_berr, _: 3;
        pub kr_pcs_block_lock, _: 4;
        pub an_next_page_recv, _: 5;
        pub an_page_received, _: 6;
        pub link_status, set_link_status: 7;
        pub kx4_sig_det, _: 11, 8;
        pub kr_sig_det, _: 12;
        pub teng_lane_sync_status, _: 16, 13;
        pub teng_align_status, _: 17;
        pub oneg_sync_status, _: 18;
        pub an_recv_idle, _: 19;
        pub oneg_an_en, _: 20;
        pub oneg_link_en_pcs, _: 21;
        pub teng_link_en, _: 22;
        pub fec_en, _: 23;
        pub teng_ser_en, set_teng_ser_en: 24;
        pub sgmii_en, _: 25;
        pub mlink_mode, set_mlink_mode: 27, 26;
        pub link_speed, set_link_speed: 29, 28;
        pub link_up, set_link_up: 30;
        pub an_complete, _: 31;
    }

    impl Default for Links {
        fn default() -> Links {
            Links(0)
        }
    }

    bitfield! {
        #[derive(Copy, Clone, Debug)]
        pub struct Rxctrl(u32);
        pub rxen, _: 0;
        pub reserved, _: 31, 1;
    }

    impl Default for Rxctrl {
        fn default() -> Rxctrl {
            Rxctrl(0)
        }
    }

    impl WriteRegister for Rxctrl {
        fn assert_valid(&self) {
            assert!(self.reserved() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Rxpbsize(u32);
        pub reserved0, _: 9, 0;
        pub size, set_size: 19, 10;
        pub reserved1, _: 31, 20;
    }

    impl Default for Rxpbsize {
        fn default() -> Rxpbsize {
            let mut val = Rxpbsize(0);
            val.set_size(0x200);
            val
        }
    }

    impl WriteRegister for Rxpbsize {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Hlreg0(u32);
        pub txcrcen, set_txcrcen: 0;
        pub reserved0, set_reserved0: 1;
        pub rxcrcstrp, set_rxcrcstrp: 2;
        pub jumboen, _: 3;
        pub reserved1, set_reserved1: 9, 3;
        pub txpaden, set_txpaden: 10;
        pub reserved2, set_reserved2: 14, 11;
        pub lpbk, _: 15;
        pub mdcspd, set_mdcspd: 16;
        pub contmdc, _: 17;
        pub reserved3, _: 19, 18;
        pub preprend, _: 23, 20;
        pub reserved4, _: 26, 24;
        pub rxlngtherren, set_rxlngtherren: 27;
        pub rxpadstripen, _: 28;
        pub reserved5, _: 31, 29;
    }

    impl Default for Hlreg0 {
        fn default() -> Hlreg0 {
            let mut val = Hlreg0(0);
            val.set_txcrcen(true);
            val.set_reserved0(true);
            val.set_rxcrcstrp(true);
            val.set_reserved1(0x1);
            val.set_txpaden(true);
            val.set_reserved2(0b101);
            val.set_mdcspd(true);
            val.set_rxlngtherren(true);
            val
        }
    }

    impl WriteRegister for Hlreg0 {
        fn assert_valid(&self) {
           assert!(self.reserved0() == true);
           assert!(self.reserved1() == 0x1);
           assert!(self.reserved2() == 0b101);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Fctrl(u32);
        pub reserved0, _: 0;
        pub sbp, _: 1;
        pub reserved1, _: 7, 2;
        pub mpe, _: 8;
        pub upe, _: 8;
        pub bam, _: 8;
        pub reserved2, _: 31, 11;
    }

    impl Default for Fctrl {
        fn default() -> Fctrl {
            Fctrl(0)
        }
    }

    impl WriteRegister for Fctrl {
        fn assert_valid(&self) {
            assert!(self.reserved0() == false);
            assert!(self.reserved1() == 0);
            assert!(self.reserved2() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Srrctl(u32);
        pub bsizepacket, set_bsizepacket: 4, 0;
        pub reserved0, _: 7,5;
        pub bsizeheader, set_bsizeheader: 13, 8;
        pub reserved1, _: 21, 14;
        pub rdmts, _: 24, 22;
        pub desctype, _: 27, 25;
        pub drop_en, _: 28;
        pub reserved2, _: 31, 29;
    }

    impl Default for Srrctl {
        fn default() -> Srrctl {
            let mut val = Srrctl(0);
            val.set_bsizepacket(0x2);
            val.set_bsizeheader(0x4);
            val
        }
    }

    impl WriteRegister for Srrctl {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == 0);
            assert!(self.reserved2() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct DescriptorLen(u32);
        len, _: 19, 0;
        reserved, _: 31, 20;
    }

    impl Default for DescriptorLen {
        fn default() -> Self {
            DescriptorLen(0)
        }
    }

    impl WriteRegister for DescriptorLen {
        fn assert_valid(&self) {
            assert!(self.reserved() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct QueuePointer(u32);
        pub pos, _: 15, 0;
        pub reserved, _: 31, 16;
    }

    impl Default for QueuePointer {
        fn default() -> Self {
            QueuePointer(0)
        }
    }

    impl WriteRegister for QueuePointer {
        fn assert_valid(&self) {
            assert!(self.reserved() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct CtrlExt(u32);
        pub reserved0, _: 13, 0;
        pub pfrstd, _: 14;
        pub reserved1, _: 14;
        pub ns_dis, _: 16;
        pub ro_dis, _: 17;
        pub reserved2, _: 25, 18;
        pub extended_vlan, _: 26;
        pub reserved3, _: 27;
        pub drv_load, _: 28;
        pub reserved4, _: 31, 29;
    }

    impl Default for CtrlExt {
        fn default() -> Self {
            CtrlExt(0)
        }
    }

    impl WriteRegister for CtrlExt {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == false);
            assert!(self.reserved2() == 0);
            assert!(self.reserved3() == false);
            assert!(self.reserved4() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct DcaRxctrl(u32);
        pub reserved0, _: 4,0;
        pub rx_desc_dca_en, _: 5;
        pub rx_header_dca_en, _: 6;
        pub rx_payload_dca_en, _: 7;
        pub reserved1, _: 8;
        pub rx_desc_read_ro_en, set_rx_desc_read_ro_en: 9;
        pub reserved2, _: 10;
        pub rx_desc_wbro_en, _: 11;
        pub reserved3, set_reserved3: 12;
        pub rx_data_write_ro_en, _: 13;
        pub reserved4, _: 14;
        pub rx_rep_header_ro_en, set_rx_rep_header_ro_en: 15;
        pub reserved5, _: 23, 16;
        pub cpuid, _: 31, 24;
    }

    impl Default for DcaRxctrl {
        fn default() -> Self {
            let mut val = DcaRxctrl(0);
            val.set_rx_desc_read_ro_en(true);
            val.set_reserved3(true);
            val.set_rx_rep_header_ro_en(true);
            val
        }
    }

    impl WriteRegister for DcaRxctrl {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == false);
            assert!(self.reserved2() == false);
            assert!(self.reserved4() == false);
            assert!(self.reserved5() == 0);
        }
    }


    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Mrqc(u32);
        pub mrqe, _: 3,0;
        pub reserved0, _: 14, 4;
        pub reserved, _: 15;
        pub rss_field_en, _: 31, 16;
    }

    impl Default for Mrqc {
        fn default() -> Self {
            Mrqc(0)
        }
    }

    impl WriteRegister for Mrqc {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.mrqe() != 0b0110);
            assert!(self.mrqe() != 0b0111);
            assert!(self.mrqe() != 0b1110);
            assert!(self.mrqe() != 0b1111);
            // Get the two reserved fields out
            assert!(self.rss_field_en() & 0b1111111100001100 == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Pfvtctl(u32);
        pub vt_ena, _: 0;
        pub reserved0, _: 6, 1;
        pub def_pl, _: 12, 7;
        pub reserved1, _: 28, 13;
        pub dis_def_pool, _: 29;
        pub rpl_en, _: 30;
        pub reserved2, _: 31;
    }

    impl Default for Pfvtctl {
        fn default() -> Self {
            Pfvtctl(0)
        }
    }

    impl WriteRegister for Pfvtctl {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == 0);
            assert!(self.reserved2() == false);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Rttup2tc(u32);
        pub up0map, _: 2, 0;
        pub up1map, _: 5, 3;
        pub up2map, _: 8, 6;
        pub up3map, _: 11, 9;
        pub up4map, _: 14, 12;
        pub up5map, _: 17, 15;
        pub up6map, _: 20, 18;
        pub up7map, _: 23, 21;
        pub reserved, _: 31, 24;
    }

    impl Default for Rttup2tc {
        fn default() -> Self {
            Rttup2tc(0)
        }
    }

    impl WriteRegister for Rttup2tc {
        fn assert_valid(&self) {
            assert!(self.reserved() == 0);
        }
    }

    bitfield! {
        #[derive(Clone, Copy)]
        pub struct Rttdcs(u32);
        pub tdpac, _: 0;
        pub vmpac, _: 1;
        pub reserved0, _: 3,2;
        pub tdrm, _: 4;
        pub reserved1, _: 5;
        pub arbdis, _: 6;
        pub reserved2, _: 16, 7;
        pub lttdesc, _: 19, 17;
        pub reserved3, _: 21, 20;
        pub bdpm, set_bdpm: 22;
        pub bpbfsm, set_bpbfsm: 23;
        pub reserved4, _: 30, 24;
        pub speed_chg, _: 31;
    }

    impl Default for Rttdcs {
        fn default() -> Self {
            let mut val = Rttdcs(0);
            val.set_bdpm(true);
            val.set_bpbfsm(true);
            val
        }
    }

    impl WriteRegister for Rttdcs {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == false);
            assert!(self.reserved2() == 0);
            assert!(self.reserved3() == 0);
            assert!(self.reserved4() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Txpbsize(u32);
        pub reserved0, _: 9, 0;
        pub size, set_size: 19, 10;
        pub reserved1, _: 31, 20;
    }

    impl Default for Txpbsize {
        fn default() -> Self {
            let mut val = Txpbsize(0);
            val.set_size(0xa0);
            val
        }
    }

    impl WriteRegister for Txpbsize {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Dtxmxszrq(u32);
        pub max_bytes_num_req, set_max_bytes_num_req: 11, 0;
        pub reserved0, _: 31, 21;
    }

    impl Default for Dtxmxszrq {
        fn default() -> Self {
            let mut val = Dtxmxszrq(0);
            val.set_max_bytes_num_req(0x10); 
            val
        }
    }

    impl WriteRegister for Dtxmxszrq {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Txpbthresh(u32);
        pub thresh, set_thresh: 9, 0;
        pub reserved0, _: 31, 10;
    }

    impl WriteRegister for Txpbthresh {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
        }
    }

    bitfield! {
        #[derive(Copy, Clone)]
        pub struct Txdctl(u32);
        pub pthresh, _: 6,0;
        pub reserved0, _: 7;
        pub hthresh, _: 14,8;
        pub reserved1, _: 15;
        pub wthresh, _: 22, 16;
        pub reserved2, _: 24, 23;
        pub enable, set_enable: 25;
        pub swflsh, _: 26;
        pub reserved3, _: 31, 27;
    }

    impl Default for Txdctl {
        fn default() -> Self {
            Txdctl(0)
        }
    }

    impl WriteRegister for Txdctl {
        fn assert_valid(&self) {
            assert!(self.reserved0() == false);
            assert!(self.reserved1() == false);
            assert!(self.reserved2() == 0);
            assert!(self.reserved3() == 0);
        }
    }

    bitfield! {
        pub struct Dmatxctl(u32);
        pub te, _: 0;
        pub reserved0, _: 1;
        pub reserved1, set_reserved1: 2;
        pub gdb, _: 3;
        pub reserved2, _: 15, 4;
        pub vt, set_vt: 31, 16;
    }

    impl Default for Dmatxctl {
        fn default() -> Self {
            let mut val = Dmatxctl(0);
            val.set_reserved1(true);
            val.set_vt(0x8100);
            val
        }
    }

    impl WriteRegister for Dmatxctl {
        fn assert_valid(&self) {
            assert!(self.reserved0() == false);
            assert!(self.reserved1() == true);
            assert!(self.reserved2() == 0);
            assert!(self.vt() == 0x8100);
        }
    }

    bitfield! {
        pub struct Rxdctl(u32);
        pub reserved0, _: 24, 0;
        pub enable, set_enable: 25;
        pub reserved1, _: 29, 26;
        pub vme, _: 30;
        pub reserved2, _: 31;
    }

    impl Default for Rxdctl {
        fn default() -> Self {
            Rxdctl(0)
        }
    }

    impl WriteRegister for Rxdctl {
        fn assert_valid(&self) {
            assert!(self.reserved0() == 0);
            assert!(self.reserved1() == 0);
            assert!(self.reserved2() == false);

            // unsupported
            assert!(self.vme() == false);
        }
    }
}