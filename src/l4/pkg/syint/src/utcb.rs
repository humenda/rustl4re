use std::mem::transmute;

use syint_cc::*;

tests! {
    fn msgtag_reimplementation_work_the_same_way() {
        use l4_sys::{l4_msgtag_w, msgtag};
        unsafe {
            assert_eq!(l4_msgtag_w(1, 2, 3, 4).raw, msgtag(1,2,3,4).raw);
            assert_eq!(l4_msgtag_w(4, 3, 2, 1).raw, msgtag(4,3,2,1).raw);
        }
    }

    fn msgtagwords_reimplementation_work_the_same_way() {
        use l4_sys::{msgtag, l4_msgtag_words_w, msgtag_words};
        let tag = msgtag(1, 2, 3, 4);
        unsafe {
            assert_eq!(l4_msgtag_words_w(tag),
                    msgtag_words(tag));
            let tag = msgtag(4, 3, 2, 1);
            assert_eq!(l4_msgtag_words_w(tag), msgtag_words(tag));
        }
    }

        // ToDo: make it use actual wrapper from libl4-wrapper, these functions need to be
        // re-exported as non-inline functions, first
//    fn msgtagflags_reimplementation_work_the_same_way() {
//        use l4_sys::{l4_msgtag, l4_msgtag_flags, msgtag_flags};
//        unsafe {
//            assert_eq!(l4_msgtag_flags(l4_msgtag(1, 2, 3, 4)),
//                    msgtag_flags(l4_msgtag(1,2,3,4)));
//            assert_eq!(l4_msgtag_flags(l4_msgtag(4, 3, 2, 1)),
//                    msgtag_flags(l4_msgtag(4,3,2,1)));
//        }
//    }

    fn serialisation_of_long_float_bool_same_as_cc() {
        use l4::utcb::*;
        use l4_sys::{l4_msg_regs_t, L4_UTCB_GENERIC_DATA_SIZE};
        let mut mr = l4_msg_regs_t {
            mr: [0u64; L4_UTCB_GENERIC_DATA_SIZE as usize]
        };
        let mut msg = unsafe {
            Msg::from_raw_mr(&mut mr as *mut l4_msg_regs_t)
        };
        unsafe {
            msg.write(987654u64).expect("Writing failed");
            msg.write(3.14f32).expect("ToDo");
            msg.write(true).expect("ToDo");
        }

        unsafe {
            let c_res = transmute::<*mut u8, *mut u64>(write_long_float_bool());
            assert_eq!(*c_res, *mr.mr.get(0).unwrap());
            assert_eq!(*c_res.offset(1), *mr.mr.get(1).unwrap());
        }
    }

    fn serialisation_of_bool_long_float_match() {
        use l4::utcb::*;
        use l4_sys::{l4_msg_regs_t, L4_UTCB_GENERIC_DATA_SIZE};
        let mut mr = l4_msg_regs_t {
            mr: [0u64; L4_UTCB_GENERIC_DATA_SIZE as usize]
        };
        let mut msg = unsafe {
            Msg::from_raw_mr(&mut mr as *mut l4_msg_regs_t)
        };
        unsafe {
            msg.write(true).expect("ToDo");
            msg.write(987654u64).expect("Writing failed");
            msg.write(3.14f32).expect("ToDo");
        }

        unsafe {
            let c_res = transmute::<*mut u8, *mut u64>(write_bool_long_float());
            assert_eq!(*c_res, *mr.mr.get(0).unwrap());
            assert_eq!(*c_res.offset(1), *mr.mr.get(1).unwrap());
            assert_eq!(*c_res.offset(2), *mr.mr.get(2).unwrap());
        }
    }
}
