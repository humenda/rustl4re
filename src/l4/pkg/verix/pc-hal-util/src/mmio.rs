#[macro_export]
macro_rules! mm2types {
    (@field_bounds $bit:literal) => {
        ($bit, $bit)
    };

    (@field_bounds $end:literal : $start:literal) => {
        ($end, $start)
    };

    (@width2num Bit32) => { 32 };
    (@width2num Bit64) => { 64 };
    (@width2ty Bit32) => { u32 };
    (@width2ty Bit64) => { u64 };

    (@reg_spec_accessor $width:ident, RO, $($field:ident @ $w1:literal $(: $w2:literal)? = $default:literal),+) => {
        pub struct R {
            val: mm2types!(@width2ty $width)
        }

        impl R {
            pub fn bits(&self) -> mm2types!(@width2ty $width) {
                self.val
            }

            $( // fields
            // TODO: This type can be improved
                pub fn $field(&self) -> mm2types!(@width2ty $width) {
                    // There is probably a more clever way to calculate this?
                    // I do however believe that LLVM should be capable of optimizing
                    let mut mask = <mm2types!(@width2ty $width)>::MAX;
                    let (end, start) = mm2types!(@field_bounds $w1 $(: $w2)?);
                    mask = (mask >> start) << start; // Get rid of the first n bits
                    let help = mm2types!(@width2num $width) - 1 - end;
                    mask = (mask << help) >> help; // Get rid of the last m bits
                    let slice = self.val & mask;
                    return slice >> start;
                }
            )+
        }
    };

    (@reg_spec_accessor $width:ident, WO, $($field:ident @ $w1:literal $(: $w2:literal)?),+) => {
        pub struct W {
            val: mm2types!(@width2ty $width)
        }

        impl W {
            $(
                // TODO: This type can be improved
                pub fn $field(mut self, val: mm2types!(@width2ty $width)) -> Self {
                    // There is probably a more clever way to calculate this?
                    // I do however believe that LLVM should be capable of optimizing
                    let mut mask = <mm2types!(@width2ty $width)>::MAX;
                    let (end, start) = mm2types!(@field_bounds $w1 $(: $w2)?);
                    mask = (mask >> start) << start; // Get rid of the first n bits
                    let help = mm2types!(@width2num $width) - 1 - end;
                    mask = (mask << help) >> help; // Get rid of the last m bits
                    let mut shifted_val = val << start;
                    // TODO: we could have an assertion here that checks that
                    // shifted_val & mask = shifted_val
                    // which would be the definition of a correct val
                    // this could be a verification condition for kani
                    shifted_val = shifted_val & mask;
                    self.val &= !mask; // clear all bits of the field that we have so far
                    self.val |= shifted_val; // set the new bits
                    self
                }
            )+
        }
    };

    // TODO: This w1 w2 approach is a bit hacky for my taste
    (@reg_spec $width:ident, $reg:ident, [$n:literal, $translate:expr], RO, $($field:ident @ $w1:literal $(: $w2:literal)? = $default:literal),+) => {
        mm2types!(@reg_spec_accessor $width, RO, $($field @ $w1 $(: $w2)? = $default),+);

        impl<'a, IM: pc_hal::traits::MemoryInterface> Register<'a, IM> {
            pub fn read(&mut self) -> R {
                R {
                    val: unsafe { core::ptr::read_volatile(self.mem.inner.ptr().add($translate(self.nth)) as *const mm2types!(@width2ty $width)) }
                }
            }
        }
    };

    (@reg_spec $width:ident, $reg:ident, [$n:literal, $translate:expr], WO, $($field:ident @ $w1:literal $(: $w2:literal)?),+) => {
        mm2types!(@reg_spec_accessor $width, WO, $($field @ $w1 $(: $w2)?),+);

        impl<'a, IM: pc_hal::traits::MemoryInterface> Register<'a, IM> {
            pub fn write<F>(&mut self, f: F)
            where
                F: FnOnce(W) -> W
            {
                let w = W { val : 0 };
                let w_final = f(w);
                unsafe { core::ptr::write_volatile(self.mem.inner.ptr().add($translate(self.nth)) as *mut mm2types!(@width2ty $width), w_final.val) }
            }
        }
    };

    (@reg_spec $width:ident, $reg:ident, [$n:literal, $translate:expr], RW, $($field:ident @ $w1:literal $(: $w2:literal)? = $default:literal),+) => {
        mm2types!(@reg_spec $width, $reg, [$n, $translate], RO, $($field @ $w1 $(: $w2)? = $default),+);
        mm2types!(@reg_spec $width, $reg, [$n, $translate], WO, $($field @ $w1 $(: $w2)?),+);

        impl<'a, IM: pc_hal::traits::MemoryInterface> Register<'a, IM> {
            pub fn modify<F>(&mut self, f: F)
            where
                F: FnOnce(&R, W) -> W
            {
                // TODO maybe assert nth < n
                let r = self.read();
                let w = W { val : r.bits() };
                let w_final = f(&r, w);
                unsafe { core::ptr::write_volatile(self.mem.inner.ptr().add($translate(self.nth)) as *mut mm2types!(@width2ty $width), w_final.val) }
            }
        }
    };

    // TODO: This w1 w2 approach is a bit hacky for my taste
    (@reg_spec $width:ident, $reg:ident, $reg_addr:literal, RO, $($field:ident @ $w1:literal $(: $w2:literal)? = $default:literal),+) => {
        mm2types!(@reg_spec_accessor $width, RO, $($field @ $w1 $(: $w2)? = $default),+);

        impl<'a, IM: pc_hal::traits::MemoryInterface> Register<'a, IM> {
            pub fn read(&mut self) -> R {
                R {
                    val: unsafe { core::ptr::read_volatile(self.mem.inner.ptr().add($reg_addr) as *const mm2types!(@width2ty $width)) }
                }
            }
        }
    };

    (@reg_spec $width:ident, $reg:ident, $reg_addr:literal, WO, $($field:ident @ $w1:literal $(: $w2:literal)?),+) => {
        mm2types!(@reg_spec_accessor $width, WO, $($field @ $w1 $(: $w2)?),+);

        impl<'a, IM: pc_hal::traits::MemoryInterface> Register<'a, IM> {
            pub fn write<F>(&mut self, f: F)
            where
                F: FnOnce(W) -> W
            {
                let w = W { val : 0 };
                let w_final = f(w);
                unsafe { core::ptr::write_volatile(self.mem.inner.ptr().add($reg_addr) as *mut mm2types!(@width2ty $width), w_final.val) }
            }
        }
    };

    (@reg_spec $width:ident, $reg:ident, $reg_addr:literal, RW, $($field:ident @ $w1:literal $(: $w2:literal)? = $default:literal),+) => {
        mm2types!(@reg_spec $width, $reg, $reg_addr, RO, $($field @ $w1 $(: $w2)? = $default),+);
        mm2types!(@reg_spec $width, $reg, $reg_addr, WO, $($field @ $w1 $(: $w2)?),+);

        impl<'a, IM: pc_hal::traits::MemoryInterface> Register<'a, IM> {
            pub fn modify<F>(&mut self, f: F)
            where
                F: FnOnce(&R, W) -> W
            {
                let r = self.read();
                let w = W { val : r.bits() };
                let w_final = f(&r, w);
                unsafe { core::ptr::write_volatile(self.mem.inner.ptr().add($reg_addr) as *mut mm2types!(@width2ty $width), w_final.val) }
            }
        }
    };

    (@reg_decl $reg:ident, $n:literal) => {
        pub struct Register<'a, IM> {
            mem: &'a mut super::Mem<IM>,
            nth : usize
        }

        impl<IM> super::Mem<IM> {
            pub fn $reg<'a>(&'a mut self, nth: usize) -> Register<'a, IM> {
                // TODO: maybe assert nth < n ?
                Register {
                    mem: self,
                    nth: nth
                }
            }
        }
    };

    (@reg_decl $reg:ident) => {
        pub struct Register<'a, IM> {
            mem: &'a mut super::Mem<IM>
        }

        impl<IM> super::Mem<IM> {
            pub fn $reg<'a>(&'a mut self) -> Register<'a, IM> {
                Register {
                    mem: self
                }
            }
        }
    };

    (@reg_specs $width:ident, $reg:ident @ [$n:literal, $translate:expr] $perms:ident { $($descrs:tt)+ } $($tail:tt)+) => {
        pub mod $reg {
            mm2types!(@reg_decl $reg, $n);
            mm2types!(@reg_spec $width, $reg, [$n, $translate], $perms, $($descrs)+);
        }
        mm2types!(@reg_specs $width, $($tail)*);
    };

    (@reg_specs $width:ident, $reg:ident @ [$n:literal, $translate:expr] $perms:ident { $($descrs:tt)+ }) => {
        pub mod $reg {
            mm2types!(@reg_decl $reg, $n);
            mm2types!(@reg_spec $width, $reg, [$n, $translate], $perms, $($descrs)+);
        }
    };

    (@reg_specs $width:ident, $reg:ident @ $reg_addr:literal $perms:ident { $($descrs:tt)+ } $($tail:tt)+) => {
        pub mod $reg {
            mm2types!(@reg_decl $reg);
            mm2types!(@reg_spec $width, $reg, $reg_addr, $perms, $($descrs)+);
        }
        mm2types!(@reg_specs $width, $($tail)*);
    };

    (@reg_specs $width:ident, $reg:ident @ $reg_addr:literal $perms:ident { $($descrs:tt)+ }) => {
        pub mod $reg {
            mm2types!(@reg_decl $reg);
            mm2types!(@reg_spec $width, $reg, $reg_addr, $perms, $($descrs)+);
        }
    };

    ($logical:ident $width:ident { $($section:ident { $($reg_spec:tt)+ } ),+ }) => {
        pub mod $logical {
            $(
                pub mod $section {
                    pub struct Mem<IM> {
                        inner: IM
                    }

                    impl<IM> From<IM> for Mem<IM> {
                        fn from(value: IM) -> Mem<IM> {
                            new(value)
                        }
                    }

                    pub fn new<IM>(inner: IM) -> Mem<IM> {
                        Mem {
                            inner
                        }
                    }

                    mm2types!(@reg_specs $width, $($reg_spec)+);
                }
            )+
        }
    }
}

mm2types! {
    MsixDev Bit32 {
        Msix {
            tadd @ [
                // TODO: what is the correct number?
                128,
                |n| 0x0000 + n * 0x10
            ] RW {
                tadd @ 31:0 = 0b0
            }
            tuadd @ [
                // TODO: what is the correct number?
                128,
                |n| 0x0004 + n * 0x10
            ] RW {
                tuadd @ 31:0 = 0b0
            }
            tmsg @ [
                // TODO: what is the correct number?
                128,
                |n| 0x0008 + n * 0x10
            ] RW {
                tmsg @ 31:0 = 0b0
            }
            tvctrl @ [
                // TODO: what is the correct number?
                128,
                |n| 0x000C + n * 0x10
            ] RW {
                masked @ 0 = 0b0,
                reserved0 @ 31:1 = 0b0
            }
        }
    }
}