pub const VENDOR_ID : u16 = 0x8086;
pub const DEVICE_ID : u16 = 0x10fb;
pub const MAX_QUEUES : u16 = 64;


dev2types! {
    Intel82559ES {
        Bar0 {
            ctrl @ 0x000000 RW {
                reserved0 @ 1:0 = 0b0,
                pcie_master_disable @ 2 = 0b0,
                lrst @ 3 = 0b0,
                reserved2 @ 25:4 = 0b0,
                rst @ 26 = 0b0,
                reserved @ 31:27 = 0b0
            }
            status @ 0x00008 RO {
                reserved0 @ 1:0 = 0b0,
                lan_id @ 3:2 = 0b0,
                reserved1 @ 6:4 = 0b0,
                link_up @ 7 = 0b0,
                reserved2 @ 9:8 = 0b0,
                num_vfs @ 17:10 = 0b0,
                iov_active @ 18 = 0b0,
                pcie_master_enable_status @ 19 = 0b1,
                reserved3 @ 31:20 = 0b0
            }
            ledctl @ 0x00200 RW {
                led0_mode @ 3:0 = 0b0,
                reserved0 @ 4 = 0b0,
                global_blink_mode @ 5 = 0b0,
                led0_ivrt @ 6 = 0b0,
                led0_blink @ 7 = 0b0,
                led1_mode @ 11:8 = 0b1,
                reserved1 @ 13:12 = 0b0,
                led1_ivrt @ 14 = 0b0,
                led1_blink @ 15 = 0b1,
                led2_mode @ 19:16 = 0x4,
                reserved2 @ 21:20 = 0b0,
                led2_ivrt @ 22 = 0b0,
                led2_blink @ 23 = 0b0,
                led3_mode @ 27:24 = 0x5,
                reserved3 @ 29:28 = 0b0,
                led3_ivrt @ 30 = 0b0,
                led3_blink @ 31 = 0b0
            }
            eicr @ 0x00800 RW {
                rtxq @ 15:0 = 0b0,
                flow_director @ 16 = 0b0,
                rx_miss @ 17 = 0b0,
                pci_exception @ 18 = 0b0,
                mail_box @ 19 = 0b0,
                lsc @ 20 = 0b0,
                link_sec @ 21 = 0b0,
                mng @ 22 = 0b0,
                reserved0 @ 23 = 0b0,
                gpi_spd0 @ 24 = 0b0,
                gpi_spd1 @ 25 = 0b0,
                gpi_spd2 @ 26 = 0b0,
                gpi_spd3 @ 27 = 0b0,
                ecc @ 28 = 0b0,
                reserved1 @ 29 = 0b0,
                tcp_timer @ 30 = 0b0,
                other_cause_interrupt @ 31 = 0b0
            }
            eims @ 0x00880 RW {
                interrupt_enable @ 30:0 = 0b0,
                reserved0 @ 31 = 0b0
            }
            eimc @ 0x00888 WO {
                interrupt_mask @ 30:0,
                reserved0 @ 31
            }
            rdrxctl @ 0x02F00 RW {
                // bit 0 is just not described in the datasheet xd?
                crcstrip @ 1 = 0b0,
                reserved0 @ 2 = 0b0,
                dmaidone @ 3 = 0b0,
                reserved1 @ 16:4 = 0x0880,
                rscfrstsize @ 21:17 = 0x8,
                reserved @ 24:22 = 0b0,
                rscackc @ 25 = 0b0,
                fcoe_wrfix @ 26 = 0b0,
                reserved2 @ 31:27 = 0b0
            }
            gprc @ 0x04074 RO {
                gprc @ 31:0 = 0b0
            }
            gptc @ 0x04080 RO {
                gptc @ 31:0 = 0b0
            }
            gorcl @ 0x04088 RO {
                cnt_l @ 31:0 = 0b0
            }
            gorch @ 0x0408C RO {
                cnt_h @ 3:0 = 0b0,
                reserved0 @ 31:4 = 0b0
            }
            gotcl @ 0x04090 RO {
                cnt_l @ 31:0 = 0b0
            }
            gotch @ 0x04094 RO {
                cnt_h @ 3:0 = 0b0,
                reserved0 @ 31:4 = 0b0
            }
            autoc @ 0x042A0 RW {
                flu @ 0 = 0b0,
                anack2 @ 1 = 0b0,
                ansf @ 6:2 = 0b00001,
                teng_pma_pmd_parallel @ 8:7 = 0b01,
                oneg_pma_pmd_parallel @ 9 = 0b1,
                d10gmp @ 10 = 0b0,
                ratd @ 11 = 0b0,
                restart_an @ 12 = 0b0,
                lms @ 15:13 = 0b100,
                kr_support @ 16 = 0b1,
                fecr @ 17 = 0b0,
                feca @ 18 = 0b1,
                anrxat @ 22:19 = 0b0011,
                anrxdm @ 23 = 0b1,
                anrxlm @ 24 = 0b1,
                anpdt @ 26:25 = 0b00,
                rf @ 27 = 0b0,
                pb @ 29:28 = 0b00,
                kx_support @ 31:30 = 0b11

            }
            links @ 0x042A4 RO {
                kx_sig_det @ 0 = 0b0,
                fec_sig_det @ 1 = 0b0,
                fec_block_lock @ 2 = 0b0,
                kr_hi_berr @ 3 = 0b0,
                kr_pcs_block_lock @ 4 = 0b0,
                kx_kx4_kr_backplane_an_next_page @ 5 = 0b0,
                kx_kx4_kr_backplane_an_page @ 6 = 0b0,
                link_status @ 7 = 0b0,
                kx4_sig_det @ 11:8 = 0b0,
                kr_sig_det @ 12 = 0b0,
                teng_lane_sync_status @ 16:13 = 0b0,
                teng_align_status @ 17 = 0b0,
                oneg_sync_status @ 18 = 0b0,
                kx_kx4_kr_backplane_receive_idle @ 19 = 0b0,
                oneg_an_enabled @ 20 = 0b0,
                oneg_link_enabled_psg @ 21 = 0b0,
                teng_link_enabled_xgxs @ 22 = 0b0,
                fec_en @ 23 = 0b0,
                teng_ser_en @ 24 = 0b0,
                sgmii_en @ 25 = 0b0,
                mlink_mode @ 27:26 = 0b0,
                link_speed @ 29:28 = 0b0,
                link_up @ 30 = 0b0,
                kx_kx4_kr_backplane_completed @ 31 = 0b0
            }
            ral0 @ 0x0A200 RW {
                ral @ 31:0 = 0b0 // TODO it is X, i need support for this
            }
            rah0 @ 0x0A204 RW {
                rah @ 15:0 = 0b0, // TODO same as RAL
                reserved0 @ 21:16 = 0b0,
                reserved1 @ 30:22 = 0b0,
                av @ 31 = 0b0 // TODO same as RAL
            }
            eec @ 0x10010 RW {
                ee_sk @ 0 = 0b0,
                ee_cs @ 1 = 0b0,
                ee_di @ 2 = 0b0,
                ee_do @ 3 = 0b0, // TODO this is X and RO
                fwe @ 5:4 = 0b01,
                ee_req @ 6 = 0b0,
                ee_gnt @ 7 = 0b0, // TODO RO
                ee_pres @ 8 = 0b0, // TODO X and RO
                auto_rd @ 9 = 0b0, // TODO RO
                reserved0 @ 10 = 0b0,
                ee_size @ 14:11 = 0b0010, // RO
                pci_ana_done @ 15 = 0b0, // RO
                pci_core_done @ 16 = 0b0, // RO
                pci_general_done @ 17 = 0b0, // RO
                pci_func_done @ 18 = 0b0, // RO
                core_done @ 19 = 0b0, // RO
                core_csr_done @ 20 = 0b0, // RO
                mac_done @ 21 = 0b0,
                reserved1 @ 31:22 = 0b0
            }
        }
    }
}
