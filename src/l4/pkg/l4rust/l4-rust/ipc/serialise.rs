use super::types::BufferAccess;
use super::super::{
    cap::{Cap, IfaceInit, Interface},
    error::Result,
    utcb::{SndFlexPage, FpageRights, UtcbMr},
};

/// Mark a trait as serialisable by the IPC framework
///
/// This trait serves both as a marker trait and a dispatcher trait. It marks a
/// type as being serialisable in its layout to the message registers and
/// readable again. Since this procedure needs to be repeatable on the C++ side
/// as well, great care has been taken. This is usually safe for primitive
/// types, a few types from this framework and for structs with a C ABI.
///
/// The UTCB is accessed through the [`ArgAccesss<T>`](struct.ArgAccess.T.html).
/// The implementor may choose to use the default implementation which
/// transmutes the message registers at the next free and already correctly
/// aligned location and copies the data of `Self` to it. The implementor may
/// choose to override this behaviour, i.e. by using the `write_items` function
/// of the message registers, by doing internal accounting or by using the
/// kernel buffer store (if capabilities are being transfered), etc. This
/// description applies both for reading and writing. For examples of
/// implementors, see the Flex Page and Cap(ability) types.
///
/// **Note:** The implementor may trust that `ArgAccess` contains a values when
/// reading the message registers or the kernel buffer objects. It is **always**
/// the responsibility of the callee to make sure that they are valid.
pub unsafe trait Serialisable: Clone {
    /// Read a value of `Self`
    ///
    /// Read a value of `Self` from given
    /// [`UtcbMr`](../utcb/struct.UtcbMr.html), by default from the message
    /// registers. The offset and alignment are automatically chosen.
    /// The same is true for the slice of capabilities passed by the framework. It may be empty,
    /// though it is expected to hold a value when this type reads a value from it. In other words,
    /// before the framework passes the slice and the implementing type for this method is a
    /// `Cap<T>`, a capability needs to be within the slice.
    #[inline]
    unsafe fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
        mr.read::<Self>() // most types are read from here
    }

    /// Write a value of this type to the message registers
    ///
    /// Offset and alignment are automatically calculated by the `ArgAccess`.
    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write::<Self>(self)
    }

    /// Write an item for sending
    #[inline]
    unsafe fn write_item(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_item::<Self>(self)
    }
}

macro_rules! impl_serialisable {
    ($($type:ty),*) => {
        $(
            unsafe impl Serialisable for $type { }
        )*
    }
}

impl_serialisable!(i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, f32, f64,
                   bool, char);

unsafe impl Serialisable for () {
    unsafe fn read(_: &mut UtcbMr, _: &mut BufferAccess) -> Result<()> {
        Ok(())
    }

    unsafe fn write(self, _: &mut UtcbMr) -> Result<()> {
        Ok(())
    }
}

unsafe impl Serialisable for SndFlexPage {
    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_item::<Self>(self)
    }

    #[inline]
    unsafe fn read(_: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
        unimplemented!("flex pages are read by the concrete type, e.g. Cap");
    }
}


unsafe impl<T: Interface + IfaceInit + Clone> Serialisable for Cap<T> {
    /// Read a capability from the given capability slice
    ///
    /// The slice must be constructed such that the first entry is the capability to be read, this
    /// method relies on it.
    #[inline]
    unsafe fn read(_: &mut UtcbMr, bufs: &mut BufferAccess) -> Result<Self> {
        Ok(bufs.rcv_cap_unchecked())
    }

    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        let fp = SndFlexPage::from_cap(&self, FpageRights::RWX, None);
        fp.write(mr)
    }
}
