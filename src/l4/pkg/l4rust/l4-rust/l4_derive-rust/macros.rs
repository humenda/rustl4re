macro_rules! err {
    ($obj_with_span:expr, $msg:literal) => {{
        $obj_with_span.span().unstable().error($msg).emit();
        return Err(syn::Error::new($obj_with_span.span(), $msg));
    }}
}

