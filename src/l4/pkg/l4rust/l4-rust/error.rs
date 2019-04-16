use core::fmt::Formatter;
use l4_sys::{*, l4_error_code_t::*, l4_ipc_tcr_error_t::*};

use num_traits::{FromPrimitive, ToPrimitive};
use crate::utcb::Utcb;

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
             FromPrimitive::from_u64(l4_ipc_error(tag, utcb.raw))
                    .ok_or(Error::UnknownErr(l4_ipc_error(tag, utcb.raw) as i64))
        }
    }
}

#[derive(Eq, PartialEq, Debug)]
pub enum Error {
    /// Generic Error Codes
    Generic(GenericErr),
    /// errors from the Thread Control Registers
    Tcr(TcrErr),
    /// Invalid capability
    InvalidCap,
    /// Description of an invalid argument, optional error code
    InvalidArg(&'static str, Option<isize>),
    /// an illegal state was reached
    InvalidState(&'static str),
    /// Unknown error code
    UnknownErr(i64),
    /// Protocol error, custom defined protocol error labels passed with an answer using the MSG
    /// msgtag label
    Protocol(i64),
    /// Invalid string encoding (required UTF-8)
    InvalidEncoding(Option<core::str::Utf8Error>),
}

impl _core::fmt::Display for Error {
    fn fmt(&self, f: &mut Formatter) -> _core::fmt::Result {
        match self {
            Error::Generic(e) => write!(f, "Generic L4 error code: {}",
                        e.to_i64().unwrap()),
            Error::Tcr(i) => write!(f, "IPC error: {}", i.to_i64().unwrap()),
            Error::InvalidCap => write!(f, "Invalid capability"),
            Error::InvalidArg(r, _arg) => write!(f, "Invalid argument '{}'", r),
            Error::InvalidState(r) => write!(f, "Invalid state: {}", r),
            Error::UnknownErr(u) => write!(f, "Unknown error code {}", u),
            Error::Protocol(p) => write!(f, "Unknown protocol requested: {}", p),
            Error::InvalidEncoding(e) => {
                match e {
                    None => write!(f, "Invalid encoding"),
                    Some(e) => {
                        write!(f, "Invalid encoding: ")?;
                        e.fmt(f)
                    }
                }
            },
        }
    }
}

impl Error {
    pub const INVALID_CAP_CODE: i64 = -65540;
    pub const INVALID_ENCODING: i64 = -65541;

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
             Ok(e) => Error::Tcr(e),
             Err(e) => e,
         }
    }

    /// Extract IPC error code from error code
    ///
    /// Error codes from L4 IPC are masked and and need to be umasked before converted to an error.
    #[inline(always)]
    pub fn from_ipc(code: i64) -> Self {
        match code {
            Self::INVALID_CAP_CODE => return Error::InvalidCap,
            Self::INVALID_ENCODING => return Error::InvalidEncoding(None),
            _ => (),
        };
        // applying the mask makes the integer positive, cast to u32 then
        match GenericErr::from_u64(code.abs() as u64) {
            Some(tc) => Error::Generic(tc),
            None => Error::UnknownErr(code),
        }
    }

    /// Convert an enum instance into a IPC error
    ///
    /// IPC errors are transmitted using the label field of the message tag and
    /// usually negative.
    pub fn into_ipc_err(&self) -> i64 {
        match self {
            Error::Generic(err) => err.to_i64().unwrap() * -1,
            Error::Tcr(err) => err.to_i64().unwrap() * -1,
            Error::InvalidCap => -1 * Self::INVALID_CAP_CODE,
            Error::InvalidArg(_, _) | Error::InvalidState(_)
                    => -1 * GenericErr::InvalidArg.to_i64().unwrap(),
            Error::InvalidEncoding(_) => -1 * Self::INVALID_ENCODING,
            Error::UnknownErr(n) | Error::Protocol(n)
                => n * -1
        }
    }
}

#[cfg(feature="std")]
impl core::convert::From<std::string::FromUtf8Error> for Error {
    fn from(u: std::string::FromUtf8Error) -> Self {
        Error::InvalidEncoding(Some(u.utf8_error()))
    }
}
pub type Result<T> = ::_core::result::Result<T, Error>;

/// Short-hand macro for several `l4::error::Error`  cases
///
/// # Example
///
/// ```
/// l4_err!(Generic, OutOfBounds), Error::Generic(GenericErr::OutOfBounds);
/// ```
#[macro_export]
macro_rules! l4_err {
    (Generic, $err:ident) => {
        return Err($crate::error::Error::Generic($crate::error::GenericErr::$err))
    };

    (InvalidEncoding($err:expr)) => {
        return Err($crate::error::Error::InvalidEncoding($err));
    }
}

#[macro_export]
macro_rules! l4_err_if {
    ($condition:expr => $($token:tt)*) => {
        if $condition {
            l4_err!($($token)*);
        }
    }
}
