macro_rules! err {
    // something that implements spanned + an error message
    ($obj_with_span:expr, $msg:literal) => {{
        $obj_with_span.span().unstable().error($msg).emit();
        return Err(syn::Error::new($obj_with_span.span(), $msg));
    }};

    ($msg:literal) => {
        compile_error!($msg);
    }
}

macro_rules! raw_type {
    ($($raw:tt)*) => {
        syn::Type::Verbatim(syn::TypeVerbatim {
                tts: TokenStream::from_str($($raw)*).unwrap() 
        })
    }
}
