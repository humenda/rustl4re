// TODO: dedup if possible
#[macro_export]
macro_rules! dev2types {
    (@field_width $bit:literal) => {
        ($bit, $bit)
    };

    (@field_width $end:literal : $start:literal) => {
        ($end, $start)
    };

    // TODO: This w1 w2 w3 approach is a bit hacky for my taste
    (@reg_spec $bar:ident, $reg:ident, $reg_addr:literal, RO, $($field:ident @ $w1:literal $(: $w2:literal)? = $default:literal),+) => {
        pub struct R {
            val: u32
        }

        impl R {
            pub fn bits(&self) -> u32 {
                self.val
            }

            $( // fields
            // TODO: This type can be improved
                pub fn $field(&self) -> u32 {
                    // There is probably a more clever way to calculate this?
                    // I do however believe that LLVM should be capable of optimizing
                    let mut mask = u32::MAX;
                    let (end, start) = dev2types!(@field_width $w1 $(: $w2)?);
                    mask = (mask >> start) << start; // Get rid of the first n bits
                    let help = 31 - end;
                    mask = (mask << help) >> help; // Get rid of the last m bits
                    let slice = self.val & mask;
                    return slice >> start;
                }
            )+
        }


        impl<'a, IM: pc_hal::traits::IoMem> Register<'a, IM> {
            pub fn read(&mut self) -> R {
                R {
                    val: self.bar.mem.read32($reg_addr)
                }
            }
        }
    };

    (@reg_spec $bar:ident, $reg:ident, $reg_addr:literal, WO, $($field:ident @ $w1:literal $(: $w2:literal)?),+) => {
        pub struct W {
            val: u32
        }

        impl W {
            $(
                // TODO: This type can be improved
                pub fn $field(mut self, val: u32) -> Self {
                    // There is probably a more clever way to calculate this?
                    // I do however believe that LLVM should be capable of optimizing
                    let mut mask = u32::MAX;
                    let (end, start) = dev2types!(@field_width $w1 $(: $w2)?);
                    mask = (mask >> start) << start; // Get rid of the first n bits
                    let help = 31 - end;
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


        impl<'a, IM: pc_hal::traits::IoMem> Register<'a, IM> {
            pub fn write<F>(&mut self, f: F)
            where
                F: FnOnce(W) -> W
            {
                let w = W { val : 0 };
                let w_final = f(w);
                self.bar.mem.write32($reg_addr, w_final.val);
            }
        }
    };

    (@reg_spec $bar:ident, $reg:ident, $reg_addr:literal, RW, $($field:ident @ $w1:literal $(: $w2:literal)? = $default:literal),+) => {
        dev2types!(@reg_spec $bar, $reg, $reg_addr, RO, $($field @ $w1 $(: $w2)? = $default),+);
        dev2types!(@reg_spec $bar, $reg, $reg_addr, WO, $($field @ $w1 $(: $w2)?),+);

        impl<'a, IM: pc_hal::traits::IoMem> Register<'a, IM> {
            pub fn modify<F>(&mut self, f: F)
            where
                F: FnOnce(&R, W) -> W
            {
                let r = self.read();
                let w = W { val : r.bits() };
                let w_final = f(&r, w);
                self.bar.mem.write32($reg_addr, w_final.val);
            }
        }
    };
    (@reg_specs $bar:ident, $reg:ident @ $reg_addr:literal $perms:ident { $($descrs:tt)+ } $($tail:tt)+) => {
        pub mod $reg {
            pub struct Register<'a, IM: pc_hal::traits::IoMem> {
                bar: &'a mut super::$bar<IM>
            }


            impl<IM: pc_hal::traits::IoMem> super::$bar<IM> {
                pub fn $reg<'a>(&'a mut self) -> Register<'a, IM> {
                    Register {
                        bar: self
                    }
                }
            }

            dev2types!(@reg_spec $bar, $reg, $reg_addr, $perms, $($descrs)+); 
        }
        dev2types!(@reg_specs $bar, $($tail)*);
    };

    (@reg_specs $bar:ident, $reg:ident @ $reg_addr:literal $perms:ident { $($descrs:tt)+ }) => {
        pub mod $reg {
            pub struct Register<'a, IM: pc_hal::traits::IoMem> {
                bar: &'a mut super::$bar<IM>
            }

            impl<IM: pc_hal::traits::IoMem> super::$bar<IM> {
                pub fn $reg<'a>(&'a mut self) -> Register<'a, IM> {
                    Register {
                        bar: self
                    }
                }
            }

            dev2types!(@reg_spec $bar, $reg, $reg_addr, $perms, $($descrs)+); 
        }
    };

    ($dev:ident { $($bar:ident { $($reg_spec:tt)+ } ),+ }) => {
        pub mod $dev {
            $(
                pub struct $bar<IM: pc_hal::traits::IoMem> {
                    mem: IM
                }

                impl<IM: pc_hal::traits::IoMem> $bar<IM> {
                    pub fn new(mem: IM) -> Self {
                        Self {
                            mem
                        }
                    }
                }

                dev2types!(@reg_specs $bar, $($reg_spec)+);
            )+
        }
    }
}
