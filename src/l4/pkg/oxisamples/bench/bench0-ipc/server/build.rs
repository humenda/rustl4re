#![feature(proc_macro_hygiene)]
extern crate l4_derive;
use l4_derive::iface_export;

fn main() {
    iface_export!(input_file:  "../../interface.rs",
                  output_file: "../../interface.h"
                  );
}
