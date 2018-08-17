////////////////////////////////////////////////////////////////////////////////
// This file is split into two parts (search for /////): l4-sys and l4 (UTCB)-related tests
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
}
