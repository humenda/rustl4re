//! All syscalls provided by Fiasco

use crate::{
    cap::{invalid_cap, Cap, Interface},
    ipc::MsgTag,
    sys::l4_timeout_t,
    utcb::Utcb,
};

/// Simple IPC Call
///
/// Call to given destination and block for answer.
#[inline(always)]
pub fn call<T: Interface>(
    dest: &Cap<T>,
    utcb: &mut Utcb,
    tag: MsgTag,
    timeout: l4_timeout_t,
) -> MsgTag {
    unsafe {
        MsgTag::from(l4_sys::l4_ipc_call(
            dest.raw(),
            utcb.raw,
            tag.raw(),
            timeout,
        ))
    }
}

#[inline(always)]
pub fn receive<T: Interface>(object: Cap<T>, utcb: &mut Utcb, timeout: l4_timeout_t) -> MsgTag {
    MsgTag::from(unsafe { l4_sys::l4_ipc_receive(object.raw(), utcb.raw(), timeout) })
}

/// Sleep for the specified amount of time.
///
/// This submits a blocking IPC to an invalid destination with the timeout being the time to sleep.
#[inline]
pub fn sleep(timeout: l4_timeout_t) -> MsgTag {
    // SAFETY: This is currently assumed to be unsafe, ignoring UTCB concurrency.
    unsafe {
        receive(invalid_cap(), &mut Utcb::current(), timeout)
    }
}
