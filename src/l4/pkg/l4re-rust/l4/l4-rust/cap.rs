use core::ops::{Deref, DerefMut};
use l4_sys::{self,
        l4_cap_consts_t::{L4_CAP_SHIFT, L4_CAP_MASK, L4_INVALID_CAP_BIT},
        l4_cap_idx_t,
        l4_msg_item_consts_t::L4_ITEM_MAP,
        L4_fpage_rights::*,
        l4_fpage_t,
        L4_fpage_type::*,
        l4_msg_item_consts_t::*,
};

use error::{Error, Result};
use types::UMword;
use utcb;

/// C capability index for FFI interaction.
pub type CapIdx = l4_cap_idx_t;

/// Capability Interface Marker
///
/// This marker trait identifies an object to provide a service through a capability.
pub trait Interface {
    unsafe fn cap(&self) -> CapIdx;
}


/// Ability for a type to initialise a capability interface.
pub trait IfaceInit: Interface {
    /// Initialise a Interface type with the capability selector from the
    /// enclosing `Cap` type.
    fn new(parent: CapIdx) -> Self;
}

/// Untyped Capability
pub struct Untyped(CapIdx);

impl Interface for Untyped {
    unsafe fn cap(&self) -> CapIdx {
        self.0
    }
}

/// Representation of a L4 Fiasco capability
///
/// This types safely wraps the interaction with capabilities and allows for easy comparison of
/// capability handles.
pub struct Cap<T: Interface> {
    pub(crate) interface: T,
}

impl<T: Interface> Deref for Cap<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.interface
    }
}

impl<T: Interface> DerefMut for Cap<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.interface
    }
}

impl<T: Interface> PartialEq<Cap<T>> for Cap<T> {
    fn eq(&self, other: &Cap<T>) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as u64) == (other.raw() >> L4_CAP_SHIFT as u64)
        }
    }

    fn ne(&self, other: &Cap<T>) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as u64)
                != (other.raw() >> L4_CAP_SHIFT as u64)
        }
    }
}

impl<T: Interface> PartialEq<CapIdx> for Cap<T> {
    fn eq(&self, other: &CapIdx) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as usize) == (other >> L4_CAP_SHIFT as usize)
        }
    }

    fn ne(&self, other: &CapIdx) -> bool {
        unsafe {
            (self.interface.cap() >> L4_CAP_SHIFT as usize)
                != (other >> L4_CAP_SHIFT as usize)
        }
    }
}


impl<T: Interface + IfaceInit> Cap<T> {
    pub fn new(c: CapIdx) -> Cap<T> {
        Cap { interface: T::new(c) }
    }
}

impl<T: Interface> Cap<T> {
    /// Allocate new capability slot
    pub fn alloc(&mut self) -> Result<Cap<Untyped>> {
        let cap = Cap {
            interface: Untyped {
                0: unsafe { l4_sys::l4re_util_cap_alloc() }
            }
        };
        match cap.is_valid() {
            true => Ok(cap),
            false => Err(Error::InvalidCap),
        }
    }

    /// Wrap the capability in a flex page with default rights.
    ///
    /// The capability is copied into a flex page with read, write, and execute permissions.
    pub fn fpage(&self) -> Fpage {
        self.fpage_with(FpageRights::RWX)
    }

    pub fn fpage_with(&self, rights: FpageRights) -> Fpage {
        unsafe {
            Fpage::from(l4_sys::l4_obj_fpage(self.interface.cap(), 0, rights.bits() as u8))
        }
    }

    #[inline]
    pub fn is_valid(&self) -> bool {
        unsafe {
            (self.interface.cap() & L4_INVALID_CAP_BIT as u64) == 0
        }
    }

    #[inline]
    pub fn is_invalid(&self) -> bool {
        unsafe {
            (self.interface.cap() & L4_INVALID_CAP_BIT as u64) != 0
        }
    }

    /// Get a raw (OS) capability handle
    ///
    /// This function returns the raw capability handle, used to interface with
    /// the kernel and other processes. This function is marked as unsafe,
    /// because this handle allows bypassing the safety guarantees provided by
    /// the `Cap<T>` type.
    #[inline]
    pub unsafe fn raw(&self) -> CapIdx {
        self.interface.cap()
    }
}

pub fn from(c: CapIdx) -> Cap<Untyped> {
    Cap { interface: Untyped { 0: c } }
}

/// Flex page types
#[repr(u8)]
#[derive(FromPrimitive, ToPrimitive)]
pub enum FpageType {
    Special = (L4_FPAGE_SPECIAL as u8) << 4,
    Memory  = (L4_FPAGE_MEMORY as u8) << 4,
    Io      = (L4_FPAGE_IO as u8) << 4,
    Obj     = (L4_FPAGE_OBJ as u8) << 4
}

/// capability mapping type (flex page type)
#[repr(u8)]
#[derive(FromPrimitive, ToPrimitive)]
pub enum MapType {
    Map   = L4_MAP_ITEM_MAP as u8,
    Grant = L4_MAP_ITEM_GRANT as u8,
}

bitflags! {
    pub struct FpageRights: u8 {
        /// executable flex page
        const X   = L4_FPAGE_X as u8;
        /// writable flex page
        const W   = L4_FPAGE_W as u8;
        /// read-only flex page
        const R   = L4_FPAGE_RO as u8;
        /// flex page which is readable and writable
        const RW  = Self::W.bits | Self::R.bits;
        /// flex page which is readable and executable
        const RX  = Self::R.bits | Self::X.bits;
        /// flex page which is readable, executable and writable
        const RWX = Self::R.bits | Self::W.bits | Self::X.bits;
    }
}

/// Sendable flex page type
///
/// This struct holds the writable version of a flex page (for an explanation of a flex page see
/// [Fpage](struct.Fpage.html). Sending a capability requires to registers, one containing the
/// action on the page, the other the flexpage to be transfered.
#[repr(C)]
#[derive(Clone)]
pub struct SendFpage {
    /// information about base address to send, map mode, etc.
    description: u64,
    // the data to transfer, e.g. a capability
    fpage: u64
}

impl SendFpage {
    pub fn new(fpage: u64, snd_base: u64, mt: Option<MapType>) -> Self {
        // this is taken mostly unmodified from the Gen_Fpage class in `ipc_types`, though cache
        // opts and `continue` (enums from this class) are not supported
        Self {
            // AKA _base in the C++ version
            description: L4_ITEM_MAP as u64
                    | (snd_base & (!0u64 << 10))
                    | mt.map(|v| v as u64).unwrap_or_default(),
            fpage: fpage
        }
    }
}

impl utcb::Serialisable for SendFpage { }

/// Flex page type
///
/// A flex page is the representation of memory, I/O ports or kernel objects (capabilities)
/// transferable across task boundaries. Consequently, it carries additional information as access
/// rights and more.
/// See the [C API](https://l4re.org/doc/group__l4__fpage__api.html) for a more in-depth
/// discussion.
pub struct Fpage(u64);

impl ::core::convert::From<l4_fpage_t> for Fpage {
    fn from(raw: l4_fpage_t) -> Self {
        unsafe {
            Fpage(raw.raw)
        }
    }
}

impl Fpage {
    // this constant magically popps up in l4_sys/cxx/ipc_types
    const RIGHTS_MASK: UMword = 0xff;
    pub fn cap(self) -> CapIdx {
        self.0 & L4_CAP_MASK as u64
    }

    pub fn rights(&self) -> u8 {
        (self.0 & Self::RIGHTS_MASK as u64) as u8
    }
    
    pub fn sendable(self, mt: Option<MapType>) -> SendFpage {
        SendFpage::new(self.0, 0, mt)
    }
}

//  Gen_fpage(l4_fpage_t const &fp, l4_addr_t snd_base = 0,
//            Map_type map_type = Map,
//            Cacheopt cache = None, Continue cont = Last)
//  : T(L4_ITEM_MAP | (snd_base & (~0UL << 10)) | l4_umword_t(map_type) | l4_umword_t(cache)
//    | l4_umword_t(cont),
//    fp.raw)
//  {}
//   Gen_fpage(L4::Cap<void> cap, unsigned rights, Map_type map_type = Map)
//  : T(L4_ITEM_MAP | l4_umword_t(map_type) | (rights & 0xf0),
//      cap.fpage(rights).raw)
//  {}

// Send flex-page
//typedef Gen_fpage<Snd_item> Snd_fpage;
// Rcv flex-page
//typedef Gen_fpage<Buf_item> Rcv_fpage;:wq
//  /**
//   * Check if the capability has been mapped.
//   *
//   * The capability itself can then be retrieved from the cap slot
//   * that has been provided in the receive operation.
//   */
//  bool cap_received() const { return (T::_base & 0x3e) == 0x38; }
//  /**
//   * Check if a label was received instead of a mapping.
//   *
//   * For IPC gates, if the L4_RCV_ITEM_LOCAL_ID has been set, then
//   * only the label of the IPC gate will be provided if the gate
//   * is local to the receiver,
//   * i.e. the target thread of the IPC gate is in the same task as the
//   * receiving thread.
//   *
//   * The label can be retrieved with Gen_fpage::data().
//   */
//  bool id_received() const { return (T::_base & 0x3e) == 0x3c; }
//  /**
//   * Check if a local capability id has been received.
//   *
//   * If the L4_RCV_ITEM_LOCAL_ID flag has been set by the receiver,
//   * and sender and receiver are in the same task, then only
//   * the capability index is transferred.
//   *
//   * The capability can be retrieved with Gen_fpage::data(; Rust: SendFpage::fpage).
//   */
//  bool local_id_received() const { return (T::_base & 0x3e) == 0x3e; }
//
//  /**
//   * Check if the received item has the compound bit set.
//   *
//   * A set compound bit means the next message item of the same type
//   * will be mapped to the same receive buffer as this message item.
//   */
//  bool is_compound() const { return T::_base & 1; }
//  /// Return the raw flex page descriptor.
//  l4_umword_t data() const { return T::_data; }
//  /// Return the raw base descriptor.
//  l4_umword_t base_x() const { return T::_base; }

