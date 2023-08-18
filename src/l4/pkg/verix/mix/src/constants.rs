pub mod pci {
    use bitfield::bitfield;

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
        pub data: Vec<u8>,
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
        fn new(addr: u32, size: usize) -> Self {
            let mut reg = MemBarReg(0);
            let data = vec![0; size];
            reg.set_addr(addr >> BAR_ALIGNMENT);

            Self {
                data,
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
}
