pub const VENDOR_ID: u16 = 0x8086;
pub const DEVICE_ID: u16 = 0x10fb;
pub const MAX_QUEUES: u8 = 128;

mm2types! {
    Intel82559ES Bit32 {
        Bar0 {
            ctrl @ 0x000000 RW {
                reserved0 @ 1:0,
                pcie_master_disable @ 2,
                lrst @ 3,
                reserved1 @ 25:4,
                rst @ 26,
                reserved2 @ 31:27
            }
            status @ 0x00008 RO {
                reserved0 @ 1:0,
                lan_id @ 3:2,
                reserved1 @ 6:4,
                link_up @ 7,
                reserved2 @ 9:8,
                num_vfs @ 17:10,
                iov_active @ 18,
                pcie_master_enable_status @ 19,
                reserved3 @ 31:20
            }
            ctrl_ext @ 0x00018 RW {
                reserved0 @ 13:0,
                pfrstd @ 14,
                reserved1 @ 15,
                ns_dis @ 16,
                ro_dis @ 17,
                reserved2 @ 25:18,
                extended_vlan @ 26,
                reserved3 @ 27,
                drv_load @ 28,
                reserved4 @ 31:29
            }
            ledctl @ 0x00200 RW {
                led0_mode @ 3:0,
                reserved0 @ 4,
                global_blink_mode @ 5,
                led0_ivrt @ 6,
                led0_blink @ 7,
                led1_mode @ 11:8,
                reserved1 @ 13:12,
                led1_ivrt @ 14,
                led1_blink @ 15,
                led2_mode @ 19:16,
                reserved2 @ 21:20,
                led2_ivrt @ 22,
                led2_blink @ 23,
                led3_mode @ 27:24,
                reserved3 @ 29:28,
                led3_ivrt @ 30,
                led3_blink @ 31
            }
            eicr @ 0x00800 RW {
                rtxq @ 15:0,
                flow_director @ 16,
                rx_miss @ 17,
                pci_exception @ 18,
                mail_box @ 19,
                lsc @ 20,
                link_sec @ 21,
                mng @ 22,
                reserved0 @ 23,
                gpi_spd0 @ 24,
                gpi_spd1 @ 25,
                gpi_spd2 @ 26,
                gpi_spd3 @ 27,
                ecc @ 28,
                reserved1 @ 29,
                tcp_timer @ 30,
                other_cause_interrupt @ 31
            }
            eiac @ 0x00810 RW {
                rtxq_autoclear @ 15:0,
                reserved0 @ 29:16,
                tcp_timer_auto_clear @ 30,
                reserved1 @ 30
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
                reserved0 @ 2:0,
                itr_interval @ 11:3,
                reserved1 @ 14:12,
                lli_moderation @ 15,
                lli_credit @ 20:16,
                itr_counter @ 27:21,
                reserved2 @ 30:28,
                cnt_wdis @ 31
            }
            eims @ 0x00880 RW {
                interrupt_enable @ 30:0,
                reserved0 @ 31
            }
            eimc @ 0x00888 WO {
                interrupt_mask @ 30:0,
                reserved0 @ 31
            }
            gpie @ 0x00898 RW {
                sdp0_gpien @ 0,
                sdp1_gpien @ 1,
                sdp2_gpien @ 2,
                sdp3_gpien @ 3,
                multiple_msix @ 4,
                ocd @ 5,
                eimem @ 6,
                ll_interval @ 10:7,
                rsc_delay @ 13:11,
                vt_mode @ 15:14,
                reserved0 @ 29:16,
                eiame @ 30,
                pba_support @ 31
            }
            ivar @ [64, |n| 0x00900 + n * 4 ] RW {
                int_alloc0 @ 5:0,
                reserved0 @ 6,
                int_alloc_val0 @ 7,
                int_alloc1 @ 13:8,
                reserved1 @ 14,
                int_alloc_val1 @ 15,
                int_alloc2 @ 21:16,
                reserved2 @ 22,
                int_alloc_val2 @ 23,
                int_alloc3 @ 29:24,
                reserved3 @ 30,
                int_alloc_val3 @ 31
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
                rdbal @ 31:0
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
                rdbah @ 31:0
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
                len @ 19:0,
                reserved0 @ 31:20
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
                rdh @ 15:0,
                reserved0 @ 31:16
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
                rdt @ 15:0,
                reserved0 @ 31:16
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
                reserved0 @ 13:0,
                reserved1 @ 14,
                reserved2 @ 15,
                reserved3 @ 22:16,
                reserved4 @ 24:23,
                enable @ 25,
                reserved5 @ 26,
                reserved6 @ 29:27,
                vme @ 30,
                reserved7 @ 31
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
                reserved0 @ 4:0,
                rx_desc_dca_en @ 5,
                rx_header_dca_en @ 6,
                rx_payload_dca_en @ 7,
                reserved1 @ 8,
                rx_desc_read_ro_en @ 9,
                reserved2 @ 10,
                rx_desc_wb_ro_en @ 11,
                reserved3 @ 12,
                rx_data_write_ro_en @ 13,
                reserved4 @ 14,
                rx_rep_header_ro_en @ 15,
                reserved5 @ 23:16,
                cpuid @ 31:24
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
                bsizepacket @ 4:0,
                reserved0 @ 7:5,
                bsizeheader @ 13:8,
                reserved1 @ 21:14,
                rdmts @ 24:22,
                desctype @ 27:25,
                drop_en @ 28,
                reserved2 @ 31:29
            }
            rdrxctl @ 0x02F00 RW {
                // bit 0 is just not described in the datasheet xd?
                crcstrip @ 1,
                reserved0 @ 2,
                dmaidone @ 3,
                reserved1 @ 16:4,
                rscfrstsize @ 21:17,
                reserved @ 24:22,
                rscackc @ 25,
                fcoe_wrfix @ 26,
                reserved2 @ 31:27
            }
            rxctrl @ 0x03000 RW {
                rxen @ 0,
                reserved0 @ 31:1
            }
            rxpbsize @ [8, |n| 0x03C00 + n * 4] RW {
                reserved0 @ 9:0,
                size @ 19:10,
                reserved1 @ 31:20
            }
            gprc @ 0x04074 RO {
                gprc @ 31:0
            }
            gptc @ 0x04080 RO {
                gptc @ 31:0
            }
            gorcl @ 0x04088 RO {
                cnt_l @ 31:0
            }
            gorch @ 0x0408C RO {
                cnt_h @ 3:0,
                reserved0 @ 31:4
            }
            gotcl @ 0x04090 RO {
                cnt_l @ 31:0
            }
            gotch @ 0x04094 RO {
                cnt_h @ 3:0,
                reserved0 @ 31:4
            }
            hlreg0 @ 0x04240 RW {
                txcren @ 0,
                rxcrstrip @ 1,
                jumboen @ 2,
                reserved0 @ 9:3,
                txpaden @ 10,
                reserved1 @ 14:11,
                lpbk @ 15,
                mdcspd @ 16,
                contmdc @ 17,
                reserved2 @ 19:18,
                prepend @ 23:20,
                reserved3 @ 24,
                reserved4 @ 26:25,
                rxlngtherren @ 27,
                rxpadstripen @ 28,
                reserved5 @ 31:29
            }
            autoc @ 0x042A0 RW {
                flu @ 0,
                anack2 @ 1,
                ansf @ 6:2,
                teng_pma_pmd_parallel @ 8:7,
                oneg_pma_pmd_parallel @ 9,
                d10gmp @ 10,
                ratd @ 11,
                restart_an @ 12,
                lms @ 15:13,
                kr_support @ 16,
                fecr @ 17,
                feca @ 18,
                anrxat @ 22:19,
                anrxdm @ 23,
                anrxlm @ 24,
                anpdt @ 26:25,
                rf @ 27,
                pb @ 29:28,
                kx_support @ 31:30

            }
            links @ 0x042A4 RO {
                kx_sig_det @ 0,
                fec_sig_det @ 1,
                fec_block_lock @ 2,
                kr_hi_berr @ 3,
                kr_pcs_block_lock @ 4,
                kx_kx4_kr_backplane_an_next_page @ 5,
                kx_kx4_kr_backplane_an_page @ 6,
                link_status @ 7,
                kx4_sig_det @ 11:8,
                kr_sig_det @ 12,
                teng_lane_sync_status @ 16:13,
                teng_align_status @ 17,
                oneg_sync_status @ 18,
                kx_kx4_kr_backplane_receive_idle @ 19,
                oneg_an_enabled @ 20,
                oneg_link_enabled_psg @ 21,
                teng_link_enabled_xgxs @ 22,
                fec_en @ 23,
                teng_ser_en @ 24,
                sgmii_en @ 25,
                mlink_mode @ 27:26,
                link_speed @ 29:28,
                link_up @ 30,
                kx_kx4_kr_backplane_completed @ 31
            }
            autoc2 @ 0x042A8 RW {
                reserved0 @ 15:0,
                teng_pma_pmd_serial @ 17:16,
                ddpt @ 18,
                reserved1 @ 27:19,
                fasm @ 28,
                reserved2 @ 29,
                pdd @ 30,
                reserved3 @ 31
            }
            rttdcs @ 0x04900 RW {
                tdpac @ 0,
                vmpac @ 1,
                reserved0 @ 3:2,
                tdrm @ 4,
                reserved1 @ 5,
                arbdis @ 6,
                reserved2 @ 16:7,
                lttdesc @ 19:17,
                reserved3 @ 21:20,
                bdpm @ 22,
                bpbfsm @ 23,
                reserved4 @ 30:24,
                speed_chg @ 31
            }
            txpbthresh @ [
                8,
                |n| 0x04950 + n * 0x4
            ] RW {
                thresh @ 9:0,
                reserved0 @ 31:10
            }
            dmatxctl @ 0x04A80 RW {
                te @ 0,
                reserved0 @ 1,
                reserved1 @ 2,
                gdv @ 3,
                reserved2 @ 15:4,
                vt @ 31:16
            }
            fctrl @ 0x05080 RW {
                reserved0 @ 0,
                sbp @ 1,
                reserved1 @ 7:2,
                mpe @ 8,
                upe @ 9,
                bam @ 10,
                reserved2 @ 31:11
            }
            tdbal @ [
                32,
                |n| 0x06000 + n * 0x40
            ] RW {
                tdbal @ 31:0
            }
            tdbah @ [
                32,
                |n| 0x06004 + n * 0x40
            ] RW {
                tdbah @ 31:0
            }
            tdlen @ [
                32,
                |n| 0x06008 + n * 0x40
            ] RW {
                len @ 19:0,
                reserved0 @ 31:20
            }
            tdh @ [
                128,
                |n| 0x06010 + n * 0x40
            ] RW {
                tdh @ 15:0,
                reserved0 @ 31:16
            }
            tdt @ [
                128,
                |n| 0x06018 + n * 0x40
            ] RW {
                tdt @ 15:0,
                reserved0 @ 31:16
            }
            txdctl @ [
                128,
                |n| 0x06028 + n * 0x40
            ] RW {
                pthresh @ 6:0,
                reserved0 @ 7,
                hthresh @ 14:8,
                reserved1 @ 15,
                wthresh @ 22:16,
                reserved @ 24:23,
                enable @ 25,
                swflsh @ 26,
                reserved2 @ 27,
                reserved3 @ 28,
                reserved4 @ 29,
                reserved5 @ 31:30
            }
            dtxmxszrq @ 0x08100 RW {
                max_bytes_num_req @ 11:0,
                reserved0 @ 31:12
            }
            ral0 @ 0x0A200 RW {
                ral @ 31:0
            }
            rah0 @ 0x0A204 RW {
                rah @ 15:0,
                reserved0 @ 21:16,
                reserved1 @ 30:22,
                av @ 31
            }
            txpbsize @ [
                8,
                |n| 0x0CC00 + n * 4
            ] RW {
                reserved0 @ 9:0,
                size @ 19:10,
                reserved1 @ 31:20
            }
            eec @ 0x10010 RW {
                ee_sk @ 0,
                ee_cs @ 1,
                ee_di @ 2,
                ee_do @ 3,
                fwe @ 5:4,
                ee_req @ 6,
                ee_gnt @ 7,
                ee_pres @ 8,
                auto_rd @ 9,
                reserved0 @ 10,
                ee_size @ 14:11,
                pci_ana_done @ 15,
                pci_core_done @ 16,
                pci_general_done @ 17,
                pci_func_done @ 18,
                core_done @ 19,
                core_csr_done @ 20,
                mac_done @ 21,
                reserved1 @ 31:22
            }
        }
    }
}

mm2types! {
    Descriptors Bit64 {
        adv_rx_desc_read {
            pkt_addr @ 0x0 RW { pkt_addr @ 63:0 }
            hdr_addr @ 0x8 RW { hdr_addr @ 63:0 }
        },
        adv_rx_desc_wb {
            lower @ 0x0 RO {
                rss_type @ 3:0,
                packet_type @ 16:4,
                rsccnt @ 20:17,
                hdr_len @ 30:21,
                sph @ 31,
                rss_hash @ 63:32
            }
            upper @ 0x8 RO {
                extended_status @ 19:0,
                dd @ 0,
                eop @ 1,
                flm @ 2,
                vp @ 3,
                fcstat @ 5:4,
                fceofs @ 6,
                pif @ 7,
                reserved0 @ 8,
                vext @ 9,
                udpv @ 10,
                llint @ 11,
                reserved1 @ 15:12,
                ts @ 16,
                secp @ 17,
                lb @ 18,
                reserved2 @ 19,
                extended_error @ 31:20,
                pkt_len @ 47:32,
                vlan_tag @ 63:48
            }
        },
        adv_tx_desc_read {
            lower @ 0x0 RW {
                address @ 63:0
            }
            upper @ 0x8 RW {
                dtalen @ 15:0,
                reserved0 @ 17:16,
                mac @ 19:18,
                dtyp @ 23:20,
                eop @ 24,
                ifcs @ 25,
                reserved1 @ 26,
                rs @ 27,
                reserved2 @ 28,
                dext @ 29,
                vle @ 30,
                tse @ 31,
                sta @ 35:32,
                idx @ 38:36,
                cc @ 39,
                popts @ 45:40,
                paylen @ 63:46
            }
        },
        adv_tx_desc_wb {
            upper @ 0x0 RW {
                reserved @ 63:0
            }
            lower @ 0x8 RW {
                reserved0 @ 31:0,
                dd @ 32,
                reserved_sta @ 35:33,
                reserved1 @ 63:36
            }
        }
    }
}
