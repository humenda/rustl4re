//! Helpers to write L4Re applications without std support.
//!
//! We expect to have libc available and rely for it on printing.

use _core::{fmt, panic::PanicInfo};

extern "C" {
    fn puts(s: *const libc::c_char) -> i32;
}

/// A buffer for println! to invoke printf.
pub struct PrintfBuffer(pub [u8; 256]);

impl PrintfBuffer {
    const fn new() -> Self {
        Self([0; 256])
    }
}

impl fmt::Write for PrintfBuffer {
    /// Print the given string.
    ///
    /// Write any valid ASCII string. Any non-ASCII character will trigger implementation-defined
    /// behaviour of the serial UART.
    fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
        if s.len() >= 255 {
            return Err(fmt::Error {});
        }
        self.0[0..s.len()].copy_from_slice(s.as_bytes());
        self.0[s.len()] = 0;
        unsafe {
            puts((&self.0 as *const u8) as *const libc::c_char);
        }
        Ok(())
    }
}

pub static mut PRINT_BUFFER: PrintfBuffer = PrintfBuffer::new();

/// A print macro
///
/// Print a string to the console. It serves as a drop-in replacement for `std::print`.
#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {{
        use core::fmt::Write; // solely for panic handler
        #[allow(unused_unsafe)]
        // SAFETY: protected by a lock
        unsafe {
            write!($crate::nostd_helper::PRINT_BUFFER, $($arg)*).unwrap();
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

/// A panic handler, instructing rustc what to call in a `panic!()`-case without std.
#[panic_handler]
pub fn l4re_panic(p_i: &PanicInfo) -> ! {
    crate::println!("{:?}", p_i);
    println!("Spinning...");
    loop {}
}
