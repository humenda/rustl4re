tests! {
    fn msgtag_reimplementation_work_the_same_way() {
        use l4_sys::{l4_msgtag, ipc::msgtag};
        unsafe {
            assert_eq!(l4_msgtag(1, 2, 3, 4).raw, msgtag(1,2,3,4).raw);
            assert_eq!(l4_msgtag(4, 3, 2, 1).raw, msgtag(4,3,2,1).raw);
        }
    }

    fn msgtagwords_reimplementation_work_the_same_way() {
        use l4_sys::{l4_msgtag, l4_msgtag_words, ipc::msgtag_words};
        unsafe {
            assert_eq!(l4_msgtag_words(l4_msgtag(1, 2, 3, 4)),
                    msgtag_words(l4_msgtag(1,2,3,4)));
            assert_eq!(l4_msgtag_words(l4_msgtag(4, 3, 2, 1)),
                    msgtag_words(l4_msgtag(4,3,2,1)));
        }
    }}
