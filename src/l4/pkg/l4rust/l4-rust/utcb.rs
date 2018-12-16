use _core::{intrinsics::transmute,
        mem::{align_of, size_of}};

use l4_sys::{
    l4_msg_item_consts_t::*,
    L4_fpage_rights::*,
    L4_fpage_type::*,
    l4_uint64_t, l4_umword_t,
        l4_utcb, l4_utcb_br, l4_utcb_mr, l4_utcb_mr_u, l4_utcb_t, l4_msg_regs_t,
        consts::UTCB_GENERIC_DATA_SIZE};

use crate::cap::{Cap, Interface};
use crate::error::{Error, GenericErr, Result};

const UTCB_DATA_SIZE_IN_BYTES: usize = UTCB_GENERIC_DATA_SIZE
                    * size_of::<l4_umword_t>();

#[macro_export]
macro_rules! utcb_mr {
    ($utcb:ident <- $($arg:expr),*) => (
        $(println!("{}", $arg + $arg);)*
    )
}


// see union l4_msg_regs_t
const MSG_REG_COUNT: usize = UTCB_GENERIC_DATA_SIZE / 
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
    pub fn mr_raw(&self) -> &[u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &*(ptr as *const [u64; MSG_REG_COUNT])
        }
    }

    #[inline]
    pub fn mr_raw_mut(&mut self) -> &mut [u64; MSG_REG_COUNT] {
        unsafe {
            let ptr = l4_utcb_mr_u(self.raw);
            &mut *(ptr as *mut [u64; MSG_REG_COUNT])
        }
    }

    /// Create message register builder
    ///
    /// This returns a message builder for the message registers. Unlike the raw
    /// C version, this builder is capable to write byte-granular to the message
    /// registers, allowing both space and performance-optimisations due to
    /// alignment. See [Msg](struct.Msg.html) for more details.
    #[inline]
    pub fn mr(&mut self) -> Msg {
        unsafe {
            Msg::from_raw_mr(l4_utcb_mr_u(self.raw))
        }
    }


    #[inline(always)]
    pub fn bdr(&self) -> usize {
        unsafe {
            (*l4_utcb_br()).bdr as usize
        }
    }
}

pub trait Serialisable: Clone {
    #[inline]
    unsafe fn read(m: &mut Msg) -> Result<Self> {
        m.read::<Self>()
    }

    #[inline]
    unsafe fn write(m: &mut Msg, v: Self) -> Result<()> {
        m.write::<Self>(v)
    }

    #[inline]
    unsafe fn write_item(m: &mut Msg, v: Self) -> Result<()> {
        m.write_item::<Self>(v)
    }
}

macro_rules! impl_serialisable {
    ($($type:ty),*) => {
        $(
            impl Serialisable for $type { }
        )*
    }
}

impl_serialisable!(i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, f32, f64,
                   bool, char);

impl Serialisable for () {
    unsafe fn read(_: &mut Msg) -> Result<()> {
        Ok(())
    }

    unsafe fn write(_: &mut Msg, _: Self) -> Result<()> {
        Ok(())
    }
}

/// Return an aligned pointer to the next free message register
///
/// The pointer is advanced by the end of the already written message registers,
/// aligned to the required type. Returned is a
/// pointer to the type-aligned memory region.
#[inline]
unsafe fn align_with_offset<T>(ptr: *mut u8, offset: usize)
        -> (*mut u8, usize) {
    let ptr = ptr.offset(offset as isize);
    let new_offset = ptr.align_offset(align_of::<T>());
    (ptr.offset(new_offset as isize), offset + new_offset)
}

/// Convenience interface for message serialisation to the message registers
pub struct Msg {
    mr: *mut u8, // casted utcb_mr pointer
    // number of items, used to subtract from the number of written words; flex pages take up two
    // Mwords, therefore number of flex pages (AKA items) needs to be stored
    items: u32,
    offset: usize
}

impl Msg {
    pub unsafe fn from_raw_mr(mr: *mut l4_msg_regs_t) -> Msg {
        Msg {
            mr: transmute::<*mut u64, *mut u8>((*mr).mr.as_mut().as_mut_ptr()),
            offset: 0,
            items: 0
        }
    }

    pub fn new() -> Msg {
        unsafe { // safe, because *every thread MUST have* a UTCB with message registers
            Msg {
                mr: transmute::<*mut u64, *mut u8>((*l4_utcb_mr())
                           .mr.as_mut().as_mut_ptr()),
                offset: 0,
                items: 0,
            }
        }
    }

    /// Write given value to the next free, type-aligned slot
    ///
    /// The value is aligned and written into the message registers.
    pub unsafe fn write<T: Serialisable>(&mut self, val: T) 
            -> Result<()> {
        let (ptr, offset) = align_with_offset::<T>(self.mr, self.offset);
        let next = offset + size_of::<T>();
        if next > UTCB_DATA_SIZE_IN_BYTES {
            return Err(Error::Generic(GenericErr::MsgTooLong));
        }
        *transmute::<*mut u8, *mut T>(ptr) = val;
        self.offset = next; // advance offset *behind* element
        Ok(())
    }

    /// Write given item to the next free, type-aligned slot
    ///
    /// The item (e.g. a flex page) is aligned and written into the message registers.
    pub unsafe fn write_item<T: Serialisable>(&mut self, val: T) 
            -> Result<()> {
        let res = Self::write(self, val)?;
        self.items += 1;
        Ok(res)
    }

    /// Read given type from the memory region at the internal buffer + offset
    ///
    /// This function reads from the given pointer + the internal offset a value
    /// from the specified type. If a read beyond the message registers is
    /// attempted, an error is returned.
    /// This is unsafe due to a missing guarantee that
    /// the pointer to the UTCB, with which the object was constructed is valid
    /// and that the memory region behind  has the length of
    /// `UTCB_DATA_SIZE_IN_BYTES`.
    pub unsafe fn read<T: Serialisable>(&mut self)  -> Result<T> {
        let (ptr, offset) = align_with_offset::<T>(self.mr, self.offset);
        let next = offset + size_of::<T>();
        if next > UTCB_DATA_SIZE_IN_BYTES {
            return Err(Error::Generic(GenericErr::MsgTooLong));
        }
        self.offset = next;
        let val: T = (*transmute::<*mut u8, *mut T>(ptr)).clone();
        Ok(val)
    }


    #[inline]
    pub fn words(&self) -> u32 {
        (self.offset / size_of::<usize>() +
            match self.offset % size_of::<usize>() {
                0 => 0,
                _ => 1
            }) as u32 - (self.items * 2) // subtract number of typed items
        // ^ items take up two Mwords: action and the object itself
    }

    /// Number of typed items in the UTCB
    #[inline]
    pub fn items(&self) -> u32 {
        self.items
    }

    /// Reset internal offset to beginning of message registers
    #[inline]
    pub fn reset(&mut self) {
        self.offset = 0
    }
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

/// A flex page is the representation of memory, I/O ports or kernel objects (capabilities)
/// transferable across task boundaries. Consequently, it carries additional information as access
/// rights and more.
/// See the [C API](https://l4re.org/doc/group__l4__fpage__api.html) for a more in-depth
/// discussion.
/// Sending a capability requires to registers, one containing the action on the
/// page, the other the flexpage to be transfered.
#[repr(C)]
#[derive(Clone)]
pub struct FlexPage {
    /// information about base address to send, map mode, etc.
    description: u64,
    // the data to transfer, e.g. a capability
    fpage: u64
}

impl FlexPage {
    /// Construct a new flex page
    ///
    /// This constructs a new flex page from the raw value of a raw flex page
    /// (`l4_fpage_t.raw`) and additional information.
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

    pub fn from_cap<T: Interface>(c: Cap<T>, rights: FpageRights,
                mt: Option<MapType>) -> FlexPage {
        FlexPage::new(unsafe {
                ::l4_sys::l4_obj_fpage(c.cap(), 0, rights.bits() as u8).raw
            }, 0, mt)
    }
}

impl Serialisable for FlexPage {
    #[inline]
    unsafe fn write(m: &mut Msg, v: Self) -> Result<()> {
        m.write_item::<Self>(v)
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
//   * The capability can be retrieved with Gen_fpage::data(; Rust: FlexPage::fpage).
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

