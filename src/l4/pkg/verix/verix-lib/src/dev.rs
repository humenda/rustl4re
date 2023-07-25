dev2types! {
    Intel82559ES {
        Bar0 {
            ctrl @ 0x4 RW {
                reserved0 @ 1:0 = 0b0,
                pcie_master_disable @ 2:2 = 0b0,
                lrst @ 3:3 = 0b0,
                reserved2 @ 25:4 = 0b0,
                rst @ 26:26 = 0b0,
                reserved @ 31:27 = 0b0
            }
            status @ 0x8 RO {
                reserved0 @ 1:0 = 0b0,
                lan_id @ 3:2 = 0b0,
                reserved1 @ 6:4 = 0b0,
                link_up @ 7:7 = 0b0,
                reserved2 @ 9:8 = 0b0,
                num_vfs @ 17:10 = 0b0,
                iov_active @ 18:18 = 0b0,
                pcie_master_enable_status @ 19:19 = 0b1,
                reserved3 @ 31:20 = 0b0
            }
            ledctl @ 0x00200 RW {
                led0_mode @ 3:0 = 0b0,
                reserved0 @ 4:4 = 0b0,
                global_blink_mode @ 5:5 = 0b0,
                led0_ivrt @ 6:6 = 0b0,
                led0_blink @ 7:7 = 0b0,
                led1_mode @ 11:8 = 0b1,
                reserved1 @ 13:12 = 0b0,
                led1_ivrt @ 14:14 = 0b0,
                led1_blink @ 15:15 = 0b1,
                led2_mode @ 19:16 = 0x4,
                reserved2 @ 21:20 = 0b0,
                led2_ivrt @ 22:22 = 0b0,
                led2_blink @ 23:23 = 0b0,
                led3_mode @ 27:24 = 0x5,
                reserved3 @ 29:28 = 0b0,
                led3_ivrt @ 30:30 = 0b0,
                led3_blink @ 31:31 = 0b0
            }
        }
    }
}
