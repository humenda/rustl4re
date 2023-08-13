use super::super::{
    cap::{Cap, IfaceInit, Interface},
    error::Result,
    utcb::{FpageRights, SndFlexPage, UtcbMr},
};
use super::types::BufferAccess;

/// Unsafe marker trait for serialisable types
///
/// A type is serialisable if it can be represented as a type compatible with
/// one in C++ and if it can be copied  across task boundaries safely. Primitive
/// types such as bool or u32 are `Serialisable`, complex types need to the
/// [Serialiser](trait.Serialiser.html) trait.
pub unsafe trait Serialisable: Clone {}

/// (De)serialisation for complex data types
///
/// This trait serves both as a serialisation and dispatcher trait. It marks a
/// type as being serialisable in its layout to the message registers and
/// readable again. Since this procedure needs to be repeatable on the C++ side
/// as well, great care has been taken. This is usually safe for primitive
/// types, a few types from this framework and for structs with a C ABI.
///
/// # Safety
///
/// The implementor must make sure that the type does not contain references, pointers, and no
/// non-primitive types.
/// An exception is if the references can be resolved while writing. For instance, when writing a
/// vector.
pub unsafe trait Serialiser
where
    Self: Sized,
{
    /// Read a value of `Self`
    ///
    /// Read a value of `Self` from given
    /// [`UtcbMr`](../utcb/struct.UtcbMr.html), by default from the message
    /// registers. The offset and alignment are automatically chosen.
    /// The same is true for the slice of capabilities passed by the framework. It may be empty,
    /// though it is expected to hold a value when this type reads a value from it. In other words,
    /// before the framework passes the slice and the implementing type for this method is a
    /// `Cap<T>`, a capability needs to be within the slice.
    fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self>;

    /// Write a value of this type to the message registers
    ///
    /// Offset and alignment are automatically calculated by the `ArgAccess`.
    fn write(self, mr: &mut UtcbMr) -> Result<()>;
}

macro_rules! impl_serialisable {
    ($($type:ty),*) => {
        $(
            unsafe impl Serialisable for $type { }
            unsafe impl Serialiser for $type {
                #[inline]
                fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
                    mr.read::<Self>() // most types are read from here
                }
                #[inline]
                fn write(self, mr: &mut UtcbMr) -> Result<()> {
                    mr.write::<Self>(self)
                }
            }
        )*
    }
}

impl_serialisable!(
    i8, i16, i32, i64, i128, u8, u16, u32, u64, u128, f32, f64, usize, isize, bool, char
);

unsafe impl Serialiser for () {
    #[inline]
    fn read(_: &mut UtcbMr, _: &mut BufferAccess) -> Result<()> {
        Ok(())
    }

    #[inline]
    fn write(self, _: &mut UtcbMr) -> Result<()> {
        Ok(())
    }
}

unsafe impl Serialisable for SndFlexPage {}
unsafe impl Serialiser for SndFlexPage {
    #[inline]
    fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_item::<Self>(self)
    }

    #[inline]
    fn read(_: &mut UtcbMr, _: &mut BufferAccess) -> Result<Self> {
        unimplemented!("flex pages are read by the concrete type, e.g. Cap");
    }
}

unsafe impl<T: Serialisable> Serialiser for Option<T> {
    #[inline]
    fn read(mr: &mut UtcbMr, _: &mut BufferAccess) -> Result<Option<T>> {
        let val = mr.read::<T>()?;
        mr.read::<bool>().map(|_| Some(val))
    }

    #[inline]
    fn write(self, mr: &mut UtcbMr) -> Result<()> {
        match self {
            None => {
                mr.skip::<T>()?;
                mr.write::<bool>(false)
            }
            Some(val) => {
                mr.write::<T>(val)?;
                mr.write::<bool>(true)
            }
        }
    }
}

unsafe impl<T: Interface + IfaceInit> Serialiser for Cap<T> {
    /// Read a capability.
    #[inline]
    fn read(_: &mut UtcbMr, bufs: &mut BufferAccess) -> Result<Self> {
        // SAFETY: ToDo, this awaits the resolution of #5
        unsafe {
            Ok(bufs.rcv_cap_unchecked())
        }
    }

    #[inline]
    fn write(self, mr: &mut UtcbMr) -> Result<()> {
        let fp = SndFlexPage::from_cap(&self, FpageRights::RWX, None);
        fp.write(mr)
    }
}

#[cfg(feature = "std")]
#[cfg(feature = "std")]
#[cfg(feature = "std")]
unsafe impl Serialiser for std::string::String {
    #[inline]
    fn read(mr: &mut UtcbMr, _x: &mut BufferAccess) -> Result<Self> {
        use std::borrow::ToOwned;
        // SAFETY: Reading a string slice is unsafe as the UTCB may be overridden at any point.
        // Since we allocate a string straight after acquiring the slice, it is turned into a safe
        // operation.
        unsafe {
            Ok(mr.read_str()?.to_owned())
        }
    }

    #[inline]
    fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_str(&self)
    }
}

#[cfg(feature = "std")]
unsafe impl<T: Serialisable> Serialiser for std::vec::Vec<T> {
    #[inline]
    fn read(mr: &mut UtcbMr, x: &mut BufferAccess) -> Result<Self> {
        super::types::BufArray::read(mr, x).map(|v| v.into())
    }

    #[inline]
    fn write(self, mr: &mut UtcbMr) -> Result<()> {
        mr.write_slice::<u16, T>(self.as_slice())
    }
}
