use l4::utcb::*;
use std::mem::transmute;

use helpers::MsgMrFake;
use syint_cc::*;

fn mk_msg_regs() -> (MsgMrFake, Msg) {
    let mut mr = MsgMrFake::new();
    let msg = unsafe {
        Msg::from_raw_mr(mr.mut_ptr())
    };
    (mr, msg)
}

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
        let mut mr = MsgMrFake::new();
        let mut msg = unsafe {
            Msg::from_raw_mr(mr.mut_ptr())
        };
        unsafe {
            msg.write(987654u64).expect("Writing failed");
            msg.write(3.14f32).expect("ToDo");
            msg.write(true).expect("ToDo");
        }

        unsafe {
            let c_res = transmute::<*mut u8, *mut u64>(write_long_float_bool());
            assert_eq!(*c_res, mr.get(0));
            assert_eq!(*c_res.offset(1), mr.get(1));
        }
    }

    fn word_count_correct_for_u64() {
        let (mr, mut msg) = mk_msg_regs();
        unsafe {
            msg.write(0u64).expect("Writing failed");
            msg.write(3u64).expect("Couldn't write to msg");
        }
        assert_eq!(msg.words(), 2);
    }

    fn word_count_is_correctly_rounded_up() {
        let (mr, mut msg) = mk_msg_regs();
        unsafe { msg.write(08).expect("Writing failed"); }
        assert_eq!(msg.words(), 1);
        unsafe { msg.write(42u8).expect("Writing failed"); }
        assert_eq!(msg.words(), 1);
        unsafe { msg.write(42u8).expect("Writing failed"); }
        assert_eq!(msg.words(), 1);
    }
}
