/// this spaghetti code just counts statically the number of token trees supplied; the manual
/// repetition helps to allow for around ~1,200 tokens to be processed
macro_rules! count_tts {
    ($_a:tt $_b:tt $_c:tt $_d:tt $_e:tt
     $_f:tt $_g:tt $_h:tt $_i:tt $_j:tt
     $_k:tt $_l:tt $_m:tt $_n:tt $_o:tt
     $_p:tt $_q:tt $_r:tt $_s:tt $_t:tt
     $($tail:tt)*)
        => {20usize + count_tts!($($tail)*)};
    ($_a:tt $_b:tt $_c:tt $_d:tt $_e:tt
     $_f:tt $_g:tt $_h:tt $_i:tt $_j:tt
     $($tail:tt)*)
        => {10usize + count_tts!($($tail)*)};
    ($_a:tt $_b:tt $_c:tt $_d:tt $_e:tt
     $($tail:tt)*)
        => {5usize + count_tts!($($tail)*)};
    ($_a:tt
     $($tail:tt)*)
        => {1usize + count_tts!($($tail)*)};
    () => {0usize};
}

macro_rules! tests {
    ($(fn $test_name:ident() $body:block)*) => {
        pub static TEST_FUNCTIONS: [fn() -> Result<(), String>;
                count_tts!($($test_name)*)] = [$($test_name,)*];
        pub static TEST_FUNCTION_NAMES: [&'static str;
                count_tts!($($test_name)*)] = [$(stringify!($test_name),)*];

        $(
            pub fn $test_name() -> Result<(), String> {
                let e = $body;
                if e == () {
                    return Ok(());
                } else {
                    return Err(format!("{:?}", e));
                }
            }
        )*
    }
}


macro_rules! assert_eq {
    ($first:expr, $second:expr) => {
        #[allow(unused_must_use)]
        match $first == $second {
            true => Ok::<(), String>(()),
            false => return Err(format!("{} != {}", $first, $second)),
        }
    }
}
