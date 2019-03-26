use l4_sys::{
    l4_msgtag as msgtag,
    l4_msgtag_t
};
use l4::{
    error::{TcrErr, Error},
    ipc::{MsgTag, MsgTagFlags}};

tests! {
    fn msg_to_short_error_correctly_parsed() {
        let msgtag = l4_msgtag_t { 
            raw: msgtag(-22, 0, 0, 0).raw | MsgTagFlags::Error as i64
        };
        if let Err(e) = MsgTag::from(msgtag).result() {
            assert_eq!(e, Error::Tcr(TcrErr::NotExistent));
        } else {
            assert_true!(false, "Expected eror, got nothing");
        }
    }
}
