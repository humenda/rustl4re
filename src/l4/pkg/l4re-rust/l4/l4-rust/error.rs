use l4_sys::{*, l4_error_code_t::*, l4_ipc_tcr_error_t::*};

use num_traits::FromPrimitive;
use utcb::Utcb;

// for the Type
enumgenerator! {
    /// Generic L4 error codes
    ///
    /// These error codes can be used both by kernel and userland programs.
    /// The enum provides a very thin wrapper around the L4 error codes to make handling them
    /// easier. They can be cast to their primitive integer value, if required for FFI.
    enum GenericErr {
        L4_EOK => Ok,
        /// No permission (`L4_EPERM`)
        L4_EPERM => NoPerm,
        /// No such entity (`L4_ENOENT`)
        L4_ENOENT        => NoEntity,
        /// I/O error (`L4_EIO`)
        L4_EIO           => IO,
        /// No such device or address (`L4_ENXIO`)
        L4_ENXIO         => NoDevOrAddr,
        /// Argument value too big (`L4_E2BIG`)
        L4_E2BIG         => Arg2Big,
        /// Try again (`L4_EAGAIN`)
        L4_EAGAIN        => TryAgain,
        /// No memory (`L4_ENOMEM`)
        L4_ENOMEM        => NoMem,
        /// Permission denied (`L4_EACCESS`)
        L4_EACCESS       => PermissionDenied,
        /// Invalid memory address (`L4_EFAULT`)
        L4_EFAULT        => InvalidMemAddr,
        /// Object currently busy, try later (`L4_EBUSY`)
        L4_EBUSY         => ObjBusy,
        /// Already exists (`L4_EEXIST`)
        L4_EEXIST        => AlreadyExists,
        /// No such thing (`L4_ENODEV`)
        L4_ENODEV        => NoDev,
        /// Invalid argument (`L4_EINVAL`)
        L4_EINVAL        => InvalidArg,
        /// No space left on devic (`L4_ENOSPC`)
        L4_ENOSPC        => NoSpace,
        /// Range error (`L4_ERANGE`)
        L4_ERANGE        => OutOfBounds,
        /// Name too long (`L4_ENAMETOOLONG`)
        L4_ENAMETOOLONG  => NameTooLong,
        /// No sys (`L4_ENOSYS`)
        L4_ENOSYS        => NoSys,
        /// Unsupported protocol (`L4_EBADPROTO`)
        L4_EBADPROTO     => InvalidProt,
        /// Address not available (`L4_EADDRNOTAVAIL`)
        L4_EADDRNOTAVAIL => AddrNotAvail,
        /// Maximum error value (`L4_ERRNOMAX`)
        L4_ERRNOMAX      => MaxErr,
        /// No reply (`L4_ENOREPLY`)
        L4_ENOREPLY      => NoReply,
        /// Message too short (`L4_EMSGTOOSHORT`)
        L4_EMSGTOOSHORT  => MsgTooShort,
        /// Message too long (`L4_EMSGTOOLONG`)
        L4_EMSGTOOLONG   => MsgTooLong,
        /// Message has invalid capability (`L4_EMSGMISSARG`)
        L4_EMSGMISSARG   => MsgInvalidCap,
        /// Communication error-range low (`L4_EIPC_LO`)
        L4_EIPC_LO       => IpcRLow,
        /// Communication error-range high (`L4_EIPC_HI`)
        L4_EIPC_HI       => IpcRHigh,
    }
}

enumgenerator! {
    /// Error codes in the *error* TCR (for IPC).
    ///
    /// The error codes are accessible via the *error* Thread Control
    /// Registers, see `l4_sys::l4_thread_regs_t`.
    enum TcrErr {
        // Mask for error bits
        L4_IPC_ERROR_MASK       => IpcErrorMask,
        /// Send error mask (`L4_IPC_SND_ERR_MASK`)
        L4_IPC_SND_ERR_MASK     => SendErrMask,
        /// Non-existing destination or source (`L4_IPC_ENOT_EXISTENT`)
        L4_IPC_ENOT_EXISTENT    => NotExistent,
        /// Timeout during receive operation (`L4_IPC_RETIMEOUT`)
        L4_IPC_RETIMEOUT        => RecvTimeout,
        /// Timeout during send operation (`L4_IPC_SETIMEOUT`)
        L4_IPC_SETIMEOUT        => SendTimeout,
        /// Receive operation canceled (`L4_IPC_RECANCELED`)
        L4_IPC_RECANCELED       => RecvCanceled,
        /// Send operation canceled (`L4_IPC_SECANCELED`)
        L4_IPC_SECANCELED       => SendCanceled,
        /// Map flexpage failed in receive operation (`L4_IPC_REMAPFAILED`)
        L4_IPC_REMAPFAILED      => RemapFailed,
        /// Map flexpage failed in send operation (`L4_IPC_SEMAPFAILED`)
        L4_IPC_SEMAPFAILED      => SendMapFailed,
        /// Send-pagefault timeout in receive operation (`L4_IPC_RESNDPFTO`)
        L4_IPC_RESNDPFTO        => RecvSendPFTimeout,
        /// Send-pagefault timeout in send operation (`L4_IPC_SESNDPFTO`)
        L4_IPC_SESNDPFTO        => SendSendPFTimeout,
        /// Receive-pagefault timeout in receive operation (`L4_IPC_RERCVPFTO`)
        L4_IPC_RERCVPFTO        => RecvRecvPFTimeout,
        /// Receive-pagefault timeout in send operation (`L4_IPC_SERCVPFTO`)
        L4_IPC_SERCVPFTO        => SendRecvPFTimeout,
        /// Receive operation aborted (`L4_IPC_REABORTED`)
        L4_IPC_REABORTED        => RecvAbort,
        /// Send operation aborted (`L4_IPC_SEABORTED`)
        L4_IPC_SEABORTED        => SendAbort,
        /// Cut receive message, due to message buffer it to small (`L4_IPC_REMSGCUT`)
        L4_IPC_REMSGCUT         => RecvMsgCut,
        /// Cut send message. due to message buffe ris too small (`L4_IPC_SEMSGCUT`)
        L4_IPC_SEMSGCUT         => SendMsgCut,
    }
}

impl TcrErr {
    /// Get error from given tag (for current thread)
    #[inline]
    pub fn from_tag(tag: l4_msgtag_t) -> Result<Self> {
        Self::from_tag_u(tag, &Utcb::current())
    }

    /// Get error from given tag and corresponding UTCB.
    #[inline]
    pub fn from_tag_u(tag: l4_msgtag_t, utcb: &Utcb) -> Result<Self> {
        unsafe {
            use num_traits::FromPrimitive;
             FromPrimitive::from_u64(l4_ipc_error(tag, utcb.raw))
                    .ok_or(Error::UnknownErr(l4_ipc_error(tag, utcb.raw) as isize))
        }
    }
}

#[derive(Debug)]
pub enum Error {
    /// Generic Error Codes
    Generic(GenericErr),
    /// errors from the Thread Control Registers
    Tcr(TcrErr),
    /// Invalid capability
    InvalidCap,
    /// Description of an invalid argument, optional error code
    InvalidArg(&'static str, Option<isize>),
    /// Unknown error code
    UnknownErr(isize),
}

impl Error {
    /// Get error from given tag (for current thread)
    #[inline]
    pub fn from_tag_raw(tag: l4_msgtag_t) -> Self {
         match TcrErr::from_tag(tag) {
             Ok(tc) => Error::Tcr(tc),
             Err(e) => e,
         }
    }
    /// Get error from given tag and corresponding UTCB.
    #[inline]
    pub fn from_tag_u(tag: l4_msgtag_t, utcb: &Utcb) -> Self {
         match TcrErr::from_tag_u(tag, utcb) {
             Ok(tc) => Error::Tcr(tc),
             Err(e) => e,
         }
    }

    /// Extract IPC error code from error code
    ///
    /// Error codes from L4 IPC are masked and and need to be umasked before converted to an error.
    #[inline(always)]
    pub fn from_ipc(code: isize) -> Self {
        // applying the mask makes the integer positive, cast to u32 then
        match TcrErr::from_isize(code & L4_IPC_ERROR_MASK as isize) {
            Some(tc) => Error::Tcr(tc),
            None => Error::UnknownErr(code & L4_IPC_ERROR_MASK as isize),
        }
    }
}

pub type Result<T> = ::core::result::Result<T, Error>;


