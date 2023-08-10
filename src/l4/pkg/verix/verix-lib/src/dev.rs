pub const VENDOR_ID: u16 = 0x8086;
pub const DEVICE_ID: u16 = 0x10fb;
pub const MAX_QUEUES: u8 = 128;

mm2types! {
    Intel82559ES Bit32 {
        Bar0 {
            ctrl @ 0x000000 RW {
                reserved0 @ 1:0 = 0b0,
                pcie_master_disable @ 2 = 0b0,
                lrst @ 3 = 0b0,
                reserved1 @ 25:4 = 0b0,
                rst @ 26 = 0b0,
                reserved2 @ 31:27 = 0b0
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
            ctrl_ext @ 0x00018 RW {
                reserved0 @ 13:0 = 0b0,
                pfrstd @ 14 = 0b0,
                reserved1 @ 15 = 0b0,
                ns_dis @ 16 = 0b0,
                ro_dis @ 17 = 0b0,
                reserved2 @ 25:18 = 0b0,
                extended_vlan @ 26 = 0b0,
                reserved3 @ 27 = 0b0,
                drv_load @ 28 = 0b0,
                reserved4 @ 31:29 = 0b0
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
            eiac @ 0x00810 RW {
                rtxq_autoclear @ 15:0 = 0b0,
                reserved0 @ 29:16 = 0b0,
                tcp_timer_auto_clear @ 30 = 0b0,
                reserved1 @ 30 = 0b0
            }
            eitr @ [
                128,
                |n|
                    if n <= 23 {
                        0x00820 + n * 4
                    } else {
                        0x012300 + ((n - 24) * 4)
                    }
            ] RW {
                reserved0 @ 2:0 = 0b0,
                itr_interval @ 11:3 = 0b0,
                reserved1 @ 14:12 = 0b0,
                lli_moderation @ 15 = 0b0,
                lli_credit @ 20:16 = 0b0,
                itr_counter @ 27:21 = 0b0,
                reserved2 @ 30:28 = 0b0,
                cnt_wdis @ 31 = 0b0
            }
            eims @ 0x00880 RW {
                interrupt_enable @ 30:0 = 0b0,
                reserved0 @ 31 = 0b0
            }
            eimc @ 0x00888 WO {
                interrupt_mask @ 30:0,
                reserved0 @ 31
            }
            gpie @ 0x00898 RW {
                sdp0_gpien @ 0 = 0b0,
                sdp1_gpien @ 1 = 0b0,
                sdp2_gpien @ 2 = 0b0,
                sdp3_gpien @ 3 = 0b0,
                multiple_msix @ 4 = 0b0,
                ocd @ 5 = 0b0,
                eimem @ 6 = 0b0,
                ll_interval @ 10:7 = 0b0,
                rsc_delay @ 13:11 = 0b0,
                vt_mode @ 15:14 = 0b0,
                reserved0 @ 29:16 = 0b0,
                eiame @ 30 = 0b0,
                pba_support @ 31 = 0b0
            }
            ivar @ [64, |n| 0x00900 + n * 4 ] RW {
                int_alloc0 @ 5:0 = 0b0,
                reserved0 @ 6 = 0b0,
                int_alloc_val0 @ 7 = 0b0,
                int_alloc1 @ 13:8 = 0b0,
                reserved1 @ 14 = 0b0,
                int_alloc_val1 @ 15 = 0b0,
                int_alloc2 @ 21:16 = 0b0,
                reserved2 @ 22 = 0b0,
                int_alloc_val2 @ 23 = 0b0,
                int_alloc3 @ 29:24 = 0b0,
                reserved3 @ 30 = 0b0,
                int_alloc_val3 @ 31 = 0b0
            }
            rdbal @ [
                128,
                |n|
                    if n < 64 {
                        0x01000 + n * 0x40
                    } else {
                        0x0D000 + ((n - 64) * 0x40)
                    }
            ] RW {
                rdbal @ 31:0 = 0b0 // TODO is X and lowest 7 bit are ignored
            }
            rdbah @ [
                128,
                |n|
                    if n < 64 {
                        0x01004 + n * 0x40
                    } else {
                        0x0D004 + ((n - 64) * 0x40)
                    }
            ] RW {
                rdbah @ 31:0 = 0b0 // TODO is X
            }
            rdlen @ [
                128,
                |n|
                    if n < 64 {
                        0x01008 + n * 0x40
                    } else {
                        0x0D008 + ((n - 64) * 0x40)
                    }
            ] RW {
                len @ 19:0 = 0b0,
                reserved0 @ 31:20 = 0b0
            }
            rdh @ [
                128,
                |n|
                    if n < 64 {
                        0x01010 + n * 0x40
                    } else {
                        0x0D010 + ((n - 64) * 0x40)
                    }
            ] RW {
                rdh @ 15:0 = 0b0,
                reserved0 @ 31:16 = 0b0
            }
            rdt @ [
                128,
                |n|
                    if n < 64 {
                        0x01018 + n * 0x40
                    } else {
                        0x0D018 + ((n - 64) * 0x40)
                    }
            ] RW {
                rdt @ 15:0 = 0b0,
                reserved0 @ 31:16 = 0b0
            }
            rxdctl @ [
                128,
                |n|
                    if n < 64 {
                        0x01028 + n * 0x40
                    } else {
                        0x0D028 + ((n - 64) * 0x40)
                    }
            ] RW {
                reserved0 @ 13:0 = 0b0,
                reserved1 @ 14 = 0b0,
                reserved2 @ 15 = 0b0,
                reserved3 @ 22:16 = 0b0,
                reserved4 @ 24:23 = 0b0,
                enable @ 25 = 0b0,
                reserved5 @ 26 = 0b0,
                reserved6 @ 29:27 = 0b0,
                vme @ 30 = 0b0,
                reserved7 @ 31 = 0b0
            }
            dca_rxctl @ [
                128,
                |n|
                        if n <= 15 {
                            0x02200 + n * 4
                        } else if n < 64 {
                            0x0100C + n * 0x40
                        } else {
                            0x0D00C + ((n - 64) * 0x40)
                        }
            ] RW {
                reserved0 @ 4:0 = 0b0,
                rx_desc_dca_en @ 5 = 0b0,
                rx_header_dca_en @ 6 = 0b0,
                rx_payload_dca_en @ 7 = 0b0,
                reserved1 @ 8 = 0b0,
                rx_desc_read_ro_en @ 9 = 0b1,
                reserved2 @ 10 = 0b0,
                rx_desc_wb_ro_en @ 11 = 0b0,
                reserved3 @ 12 = 0b1,
                rx_data_write_ro_en @ 13 = 0b1,
                reserved4 @ 14 = 0b0,
                rx_rep_header_ro_en @ 15 = 0b0,
                reserved5 @ 23:16 = 0b0,
                cpuid @ 31:24 = 0b0
            }
            srrctl @ [
                128,
                |n|
                    if n <= 15 {
                        0x02100 + n * 4
                    } else if n < 64 {
                        0x01014 + n * 0x40
                    } else {
                        0x0D014 + ((n - 64) * 0x40)
                    }
            ] RW {
                bsizepacket @ 4:0 = 0x2,
                reserved0 @ 7:5 = 0b0,
                bsizeheader @ 13:8 = 0x4,
                reserved1 @ 21:14 = 0b0,
                rdmts @ 24:22 = 0b0,
                desctype @ 27:25 = 0b0,
                drop_en @ 28 = 0b0,
                reserved2 @ 31:29 = 0b0
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
            rxctrl @ 0x03000 RW {
                rxen @ 0 = 0b0,
                reserved0 @ 31:1 = 0b0
            }
            rxpbsize @ [8, |n| 0x03C00 + n * 4] RW {
                reserved0 @ 9:0 = 0b0,
                size @ 19:10 = 0x200,
                reserved1 @ 31:20 = 0b0
            }
            gprc @ 0x04074 RO {
                gprc @ 31:0 = 0b0
            }
            gptc @ 0x04080 RO {
                gptc @ 31:0 = 0b0
            }
            txdpgc @ 0x087A0 RO {
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
            hlreg0 @ 0x04240 RW {
                txcren @ 0 = 0b1,
                rxcrstrip @ 1 = 0b1,
                jumboen @ 2 = 0b0,
                reserved0 @ 9:3 = 0x1,
                txpaden @ 10 = 0b1,
                reserved1 @ 14:11 = 0b101,
                lpbk @ 15 = 0b0,
                mdcspd @ 16 = 0b1,
                contmdc @ 17 = 0b0,
                reserved2 @ 19:18 = 0b0,
                prepend @ 23:20 = 0b0,
                reserved3 @ 24 = 0b0,
                reserved4 @ 26:25 = 0b0,
                rxlngtherren @ 27 = 0b1,
                rxpadstripen @ 28 = 0b0,
                reserved5 @ 31:29 = 0b0
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
            rttdcs @ 0x04900 RW {
                tdpac @ 0 = 0b0,
                vmpac @ 1 = 0b0,
                reserved0 @ 3:2 = 0b0,
                tdrm @ 4 = 0b0,
                reserved1 @ 5 = 0b0,
                arbdis @ 6 = 0b0,
                reserved2 @ 16:7 = 0b0,
                lttdesc @ 19:17 = 0b0,
                reserved3 @ 21:20 = 0b0,
                bdpm @ 22 = 0b1,
                bpbfsm @ 23 = 0b1,
                reserved4 @ 30:24 = 0b0,
                speed_chg @ 31 = 0b0
            }
            dmatxctl @ 0x04A80 RW {
                te @ 0 = 0b0,
                reserved0 @ 1 = 0b0,
                reserved1 @ 1 = 0b1,
                gdv @ 3 = 0b0,
                reserved2 @ 15:4 = 0b0,
                vt @ 31:16 = 0x8100
            }
            fctrl @ 0x05080 RW {
                reserved0 @ 0 = 0b0,
                sbp @ 1 = 0b0,
                reserved1 @ 7:2 = 0b0,
                mpe @ 8 = 0b0,
                upe @ 9 = 0b0,
                bam @ 10 = 0b0,
                reserved2 @ 31:11 = 0b0
            }
            tdbal @ [
                32,
                |n| 0x06000 + n * 0x40
            ] RW {
                tdbal @ 31:0 = 0b0
            }
            tdbah @ [
                32,
                |n| 0x06004 + n * 0x40
            ] RW {
                tdbah @ 31:0 = 0b0
            }
            tdlen @ [
                32,
                |n| 0x06008 + n * 0x40
            ] RW {
                len @ 19:0 = 0b0,
                reserved0 @ 31:20 = 0b0
            }
            tdh @ [
                128,
                |n| 0x06010 + n * 0x40
            ] RW {
                tdh @ 15:0 = 0b0,
                reserved0 @ 31:16 = 0b0
            }
            tdt @ [
                128,
                |n| 0x06018 + n * 0x40
            ] RW {
                tdt @ 15:0 = 0b0,
                reserved0 @ 31:16 = 0b0
            }
            txdctl @ [
                128,
                |n| 0x06028 + n * 0x40
            ] RW {
                pthresh @ 6:0 = 0b0,
                reserved0 @ 7 = 0b0,
                hthresh @ 14:8 = 0b0,
                reserved1 @ 15 = 0b0,
                wthresh @ 22:16 = 0b0,
                reserved @ 24:23 = 0b0,
                enable @ 25 = 0b0,
                swflsh @ 26 = 0b0,
                reserved2 @ 27 = 0b0,
                reserved3 @ 28 = 0b0,
                reserved4 @ 29 = 0b0,
                reserved5 @ 31:30 = 0b0
            }
            dtxmxszrq @ 0x08100 RW {
                max_bytes_num_req @ 11:0 = 0x10,
                reserved0 @ 31:12 = 0b0
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
            txpbsize @ [
                8,
                |n| 0x0CC00 + n * 4
            ] RW {
                reserved0 @ 9:0 = 0b0,
                size @ 19:10 = 0xa0,
                reserved1 @ 31:20 = 0b0
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

mm2types! {
    Descriptors Bit64 {
        adv_rx_desc_read {
            pkt_addr @ 0x0 RW { pkt_addr @ 63:0 = 0b0 }
            hdr_addr @ 0x8 RW { hdr_addr @ 63:0 = 0b0 }
        },
        adv_rx_desc_wb {
            lower @ 0x0 RO {
                rss_type @ 3:0 = 0b0,
                packet_type @ 16:4 = 0b0,
                rsccnt @ 20:17 = 0b0,
                hdr_len @ 30:21 = 0b0,
                sph @ 31 = 0b0,
                rss_hash @ 63:32 = 0b0
            }
            upper @ 0x8 RO {
                extended_status @ 19:0 = 0b0,
                dd @ 0 = 0b0,
                eop @ 1 = 0b0,
                flm @ 2 = 0b0,
                vp @ 3 = 0b0,
                fcstat @ 5:4 = 0b0,
                fceofs @ 6 = 0b0,
                pif @ 7 = 0b0,
                reserved0 @ 8 = 0b0,
                vext @ 9 = 0b0,
                udpv @ 10 = 0b0,
                llint @ 11 = 0b0,
                reserved1 @ 15:12 = 0b0,
                ts @ 16 = 0b0,
                secp @ 17 = 0b0,
                lb @ 18 = 0b0,
                eserved2 @ 19 = 0b0,
                extended_error @ 31:20 = 0b0,
                pkt_len @ 47:32 = 0b0,
                vlan_tag @ 63:48 = 0b0
            }
        },
        adv_tx_desc_read {
            lower @ 0x0 RW {
                address @ 63:0 = 0b0
            }
            upper @ 0x8 RW {
                dtalen @ 15:0 = 0b0,
                reserved0 @ 17:16 = 0b0,
                mac @ 19:18 = 0b0,
                dtyp @ 23:20 = 0b0,
                eop @ 24 = 0b0,
                ifcs @ 25 = 0b0,
                reserved1 @ 26 = 0b0,
                rs @ 27 = 0b0,
                reserved2 @ 28 = 0b0,
                dext @ 29 = 0b0,
                vle @ 30 = 0b0,
                tse @ 31 = 0b0,
                sta @ 35:32 = 0b0,
                idx @ 38:36 = 0b0,
                cc @ 39 = 0b0,
                popts @ 45:40 = 0b0,
                paylen @ 63:46 = 0b0
            }
        },
        adv_tx_desc_wb {
            upper @ 0x0 RW {
                reserved @ 63:0 = 0b0
            }
            lower @ 0x8 RW {
                reserved0 @ 31:0 = 0b0,
                dd @ 32 = 0b0,
                reserved_sta @ 35:33 = 0b0,
                reserved1 @ 63:36 = 0b0
            }
        }
    }
}
