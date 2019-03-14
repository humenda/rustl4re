use super::types::{BufferAccess};
use super::super::{
    cap::{Cap, IfaceInit, Interface},
    error::Result,
    utcb::{SndFlexPage, FpageRights, UtcbMr},
};

/// Unsafe marker trait for serialisable types
///
/// A type is serialisable if it can be represented as a type compatible with
/// one in C++ and if it can be copied  across task boundaries safely. Primitive
/// types such as bool or u32 are `Serialisable`, complex types need to the
/// [Serialiser](trait.Serialiser.html) trait.
pub unsafe trait Serialisable: Clone { }

/// (De)serialisation for complex data types
///
/// This trait serves both as a serialisation and dispatcher trait. It marks a
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
pub unsafe trait Serialiser where Self: Sized {
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
    unsafe fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self>;

    /// Write a value of this type to the message registers
    ///
    /// Offset and alignment are automatically calculated by the `ArgAccess`.
    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()>;
}

macro_rules! impl_serialisable {
    ($($type:ty),*) => {
        $(
            unsafe impl Serialisable for $type { }
            unsafe impl Serialiser for $type {
                #[inline]
                unsafe fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
                    mr.read::<Self>() // most types are read from here
                }
                #[inline]
                unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
                    mr.write::<Self>(self)
                }
            }
        )*
    }
}

impl_serialisable!(i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, f32, f64,
                   usize, isize, bool, char);

unsafe impl Serialisable for () { }
unsafe impl Serialiser for () {
    #[inline]
    unsafe fn read(_: &mut UtcbMr, _: &mut BufferAccess) -> Result<()> { Ok(()) }
    #[inline]
    unsafe fn write(self, _: &mut UtcbMr) -> Result<()> { Ok(()) }
}

unsafe impl Serialisable for SndFlexPage { }
unsafe impl Serialiser for SndFlexPage {
    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_item::<Self>(self)
    }

    #[inline]
    unsafe fn read(_: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
        unimplemented!("flex pages are read by the concrete type, e.g. Cap");
    }
}

unsafe impl<T: Serialisable> Serialiser for Option<T> {
    #[inline]
    unsafe fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Option<T>> {
        let val = mr.read::<T>()?;
        Ok(match mr.read::<bool>()? { // Option is valid?
            true => Some(val),
            false => Option::<T>::None
        })
    }

    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        match self {
            None => {
                mr.skip::<T>()?;
                mr.write::<bool>(false)
            },
            Some(val) => {
                mr.write::<T>(val)?;
                mr.write::<bool>(true)
            }
        }
    }
}

unsafe impl<T: Interface + IfaceInit> Serialiser for Cap<T> {
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

//#[cfg(feature="std")]
#[cfg(feature="std")]
unsafe impl<'a> Serialiser for &'a str {
    #[inline]
    unsafe fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
        let slice = mr.read_slice::<usize, u8>()?;
        let length = match slice.last() == Some(&0u8) {
            true => slice.len() - 1,
            false => slice.len()
        };
        core::str::from_utf8(core::slice::from_raw_parts(slice.as_ptr(),
                length))
            .map_err(|e| crate::error::Error::InvalidEncoding(Some(e)))
    }

    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_str(self)
    }
}

#[cfg(feature="std")]
#[cfg(feature="std")]
unsafe impl Serialiser for std::string::String {
    #[inline]
    unsafe fn read(mr: &mut UtcbMr, x: &mut BufferAccess) -> Result<Self> {
        Ok(std::string::String::from(<&str as Serialiser>::read(mr, x)?))
    }

    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_str(&self)
    }
}

#[cfg(feature="std")]
unsafe impl<T: Serialisable> Serialiser for std::vec::Vec<T> {
    #[inline]
    unsafe fn read(mr: &mut UtcbMr, x: &mut BufferAccess) -> Result<Self> {
        let slice: &[T] = super::types::BufArray::read(mr, x)?.into();
        Ok(std::vec::Vec::from(slice))
    }

    #[inline]
    unsafe fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_slice::<u16, T>(self.as_slice())
    }
}
