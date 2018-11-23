extern crate l4_sys;
extern crate l4;
extern crate l4re_sys;

#[macro_use]
mod macros;
mod helpers;
mod utcb_cc;


const BEGIN_MARKER: &'static str = "[[|| BEGIN TESTS ||]]";
const END_MARKER: &'static str = "[[|| END TESTS ||]]";

/// Iterate over all modules and execute tests
macro_rules! call_mod_tests {
    ($(mod $modname:ident;)*) => {
        $(
            mod $modname;
         )*
        pub fn main() {
            println!("{}", BEGIN_MARKER);
            $(
                println!("1..{}", $modname::TEST_FUNCTIONS.len());
                for (num, function) in $modname::TEST_FUNCTIONS.iter()
                        .enumerate() {
                    match function() {
                        Ok(_) => println!("ok {} - {}", num + 1,
                                          $modname::TEST_FUNCTION_NAMES[num]),
                        Err(msg) => println!("not ok {} - {}\n  {}", num + 1,
                                          $modname::TEST_FUNCTION_NAMES[num],
                                          msg.replace("\n", "\n  ")),
                    };
                }
            )*
            println!("{}", END_MARKER);
        }
    }
}

call_mod_tests! {
    mod l4sys;
    mod utcb;
}

