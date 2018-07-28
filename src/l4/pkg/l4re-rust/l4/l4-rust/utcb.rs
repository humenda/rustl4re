use std::mem::size_of;
use l4_sys::{l4_uint64_t, l4_umword_t,
    l4_utcb, l4_utcb_br, l4_utcb_mr_u, l4_utcb_t};
use l4_sys::L4_UTCB_GENERIC_DATA_SIZE;


#[macro_export]
macro_rules! utcb_mr {
    ($utcb:ident <- $($arg:expr),*) => (
        $(println!("{}", $arg + $arg);)*
    )
}


// see union l4_msg_regs_t
const MSG_REG_COUNT: usize = L4_UTCB_GENERIC_DATA_SIZE as usize / 
        (size_of::<l4_uint64_t>() / size_of::<l4_umword_t>());


// NOTE: this may never be `Send`!
/// User space Thread Control Block
///
/// The UTCB is used for storing data for IPC operations and furthermore provides means for the
/// user to interact with a thread instance. More details are to be found in the official L4Re
/// documentation.
///
/// # Example
///
/// ```
/// use l4::Utcb;
///
/// fn main() {
///     let u = Utcb()::current(); // UTCB of current thread
///     u.mr_mut()[0] = 20;
/// }
pub struct Utcb {
    pub raw: *mut l4_utcb_t, // opaque UTCB struct
}

impl Utcb {
    pub fn from_utcb(utcb: *mut l4_utcb_t) -> Utcb {
        Utcb { raw: utcb }
    }

    pub fn current() -> Utcb {
        unsafe { // this can't fail: no UTCB, no thread -> no execution
            Utcb { raw: l4_utcb() }
        }
    }

    #[inline]
    pub fn mr(&self) -> &[u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &*(ptr as *const [u64; MSG_REG_COUNT])
        }
    }

    #[inline]
    pub fn mr_mut(&mut self) -> &mut [u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &mut *(ptr as *mut [u64; MSG_REG_COUNT])
        }
    }

    #[inline(always)]
    pub fn bdr(&self) -> usize {
        unsafe {
            (*l4_utcb_br()).bdr as usize
        }
    }

    /*pub fn br(&self) -> &[usize {
        unsafe {
            let buffer_registers = (*l4_utcb_br()).br;
            &*(buffer_registers as *const [usize; L4_UTCB_GENERIC_BUFFERS_SIZE])
        }
    }
    */
}
