//! Printing if no standard library is available.
//!
//! This printing is not very elegant and mainly intended in scenarios where Rust has acces to
//! uclibc, but the Rust std hasn't been ported yet.

use core::fmt;

extern "C" {
    pub fn putchar(c: core::ffi::c_uchar) -> u32;
}

pub struct L4RustNoStdFmt;

impl core::fmt::Write for L4RustNoStdFmt {
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        for b in s.bytes() {
            unsafe {
                putchar(b);
            }
        }
        Ok(())
    }
}

/// A print macro
///
/// Print a string to the console.
#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {{
        use core::fmt::Write; // solely for panic handler
        #[allow(unused_unsafe)]
        // SAFETY: protected by a lock
        unsafe {
            write!($crate::print::L4RustNoStdFmt { }, $($arg)*).unwrap();
        }
    }}
}

/// Print a string, followed by a new line. See [crate::print!] for more information.
#[macro_export]
macro_rules! println {
    () => {
        $crate::print!("\n");
    };
    // This arm cannot be joined with the one below since it would match on a repeated *comma*.
    // This conflicts with $arg:tt already matching commas, so e.g. $(, $arg:tt)* is not possible.
    ($some_literal:literal) => {
        $crate::print!(concat!($some_literal, "\n"));
    };
    ($fmt_str:literal, $($arg:tt)*) => {
        $crate::print!(concat!($fmt_str, "\n"), $($arg)*);
    }
}
