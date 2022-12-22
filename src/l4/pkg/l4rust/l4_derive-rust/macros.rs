macro_rules! err {
    // something that implements spanned + an error message
    ($obj_with_span:expr, $msg:literal) => {{
        return Err(syn::Error::new($obj_with_span.span(), $msg));
    }};

    ($obj_with_span:expr, $msg:expr) => {{
        // same again
        return Err(syn::Error::new($obj_with_span.span(), $msg));
    }};

    ($obj_with_span:expr, $msg:ident) => {
        return Err(syn::Error::new($obj_with_span.span(), $msg));
    };

    ($msg:expr) => {
        return Err(syn::Error::new(proc_macro2::Span::call_site(), $msg))
    };
}

macro_rules! raw_type {
    ($($raw:tt)*) => {
        syn::Type::Verbatim(syn::TypeVerbatim {
                tts: TokenStream::from_str($($raw)*).unwrap()
        })
    }
}

macro_rules! ifletelse {
    ($match:expr => $path:path; $msg:expr) => {
        match $match {
            $path(ref l) => l,
            _ => err!($match, $msg),
        }
    };
}
