use proc_macro2::{Span, TokenStream};
use quote::quote;
use std::str::FromStr;
use syn::{braced,
    Ident,
    parse::{Parse, ParseStream, Result},
    spanned::Spanned,
    Token
};

macro_rules! raw_type {
    ($($raw:tt)*) => {
        syn::Type::Verbatim(syn::TypeVerbatim {
                tts: TokenStream::from_str($($raw)*).unwrap() 
        })
    }
}

// parsed interface bits
pub struct RawIface {
    pub iface_name: Ident,
    pub iface_attrs: Vec<IfaceAttr>,
    pub methods: Vec<syn::TraitItemMethod>,
}

impl Parse for RawIface {
    fn parse(input: ParseStream) -> Result<Self> {
        let _: Token![trait] = input.parse()?;
        let iface_name = input.parse()?;
        let content;
        let _brace_token = braced!(content in input);
        let mut methods = Vec::new();
        let mut iface_attrs = Vec::new();
        while !content.is_empty() {
            match content.parse::<syn::TraitItem>()? {
                syn::TraitItem::Const(c) =>
                        iface_attrs.push(IfaceAttr::Const(c)),
                syn::TraitItem::Type(c) =>
                        iface_attrs.push(IfaceAttr::Type(c)),
                syn::TraitItem::Method(c) => methods.push(c),
                thing => {
                    thing.span().unstable().error("Only methods, associated \
                            consts and associated types allowed").emit();
                    panic!();
                }
            }
        }
        Ok(RawIface {
            iface_name,
            iface_attrs,
            methods
        })
    }
}

/// IPC interface attributes
///
/// IPC interfaces offer optional and required options to further configure the interface, such as
/// the `PROTOCOL_ID`, etc.
/// We don't use TraitItem here, since we only support a trait subset.
pub enum IfaceAttr {
    Const(syn::TraitItemConst),
    Type(syn::TraitItemType)
}

impl Parse for IfaceAttr {
    fn parse(input: ParseStream) -> Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(Token![const]) {
            input.parse().map(IfaceAttr::Const)
        } else if lookahead.peek(Token![type]) {
            input.parse().map(IfaceAttr::Type)
        } else {
            Err(lookahead.error())
        }
    }
}

// duck typing
impl IfaceAttr {
    fn span(&self) -> Span {
        match self {
            IfaceAttr::Const(x) => x.span(),
            IfaceAttr::Type(x) => x.span(),
        }
    }
}

impl quote::ToTokens for IfaceAttr {
    fn to_tokens(&self, t: &mut TokenStream) {
        match self {
            IfaceAttr::Const(x) => x.to_tokens(t),
            IfaceAttr::Type(x) => x.to_tokens(t),
        };
    }

    fn into_token_stream(self) -> TokenStream
            where Self: Sized {
        match self {
            IfaceAttr::Const(t) => t.into_token_stream(),
            IfaceAttr::Type(t) => t.into_token_stream(),
        }
    }
}

// reply with an error so that function above can return empty TokenStream; the
// error already got registered globally with the error() function

pub struct ParsedIfaceAttrs {
    pub protocol: syn::Expr,
    pub opattrs: Vec<syn::Attribute>,
    pub optype: syn::Type,
}

// Interfaces can have only a handful meta attributes. One is the PROTOCOL_ID,
// which is mandatory. Optional is the OpType and if it wasn't specified, it'll
// default to i32.
pub fn parse_iface_attributes(outer: syn::Ident, attrs: &Vec<IfaceAttr>)
        -> Result<ParsedIfaceAttrs> {
//        -> Result<TokenStream> {
    let mut opattrs = Vec::new();
    // default to a OpType of i32
    let mut optype: syn::Type = raw_type!("i32");
    let mut protocol: Option<syn::Expr> = None;
    for attr in attrs.iter() {
        match attr {
            IfaceAttr::Const(c) if c.ident == "PROTOCOL_ID" => match &c.default {
                Some((_, expr)) => protocol = Some(expr.clone()),
                None => err!(c, "Missing protocol ID"),
            },
            IfaceAttr::Type(t) if t.ident == "OpType" => match &t.default {
                None => err!(t.ident, "No operand type specified"),
                Some((_, tt)) => {
                    optype = tt.clone();
                    opattrs = t.attrs.clone();
                }
            },
            _ => err!(attr, "Illegal interface attribute specified"),
        }
    }
    if protocol.is_none() {
        err!(outer, "No protocol id (PROTOCOL_ID) specified");
    }
    let protocol = protocol.unwrap().clone(); // we know it's there
    Ok(ParsedIfaceAttrs {
        protocol, opattrs, optype,
    })
}

pub fn gen_iface_attrs(if_attrs: ParsedIfaceAttrs) -> TokenStream {
    let ParsedIfaceAttrs { protocol, opattrs, optype } = if_attrs;
    quote! {
        const PROTOCOL_ID: i64 = #protocol;
        #(#opattrs)*
        type OpType = #optype;
    }
}

// ToDo: no enumeration yet
pub fn parse_iface_methods(methods: &Vec<syn::TraitItemMethod>)
        -> Result<Vec<syn::TraitItemMethod>> {
    for method in methods {
        // expected `fn(&mut self, …) -> ret;} (ret is optional)
        let sig = &method.sig;
        if sig.constness.is_some() {
            err!(sig.constness.unwrap(), "Const fn functions not allowed");
        }
        // test for &mut self
        match sig.decl.inputs.iter().next() {
            None => err!(sig, "First parameter must be &mut self"),
            Some(x) => match x {
                syn::FnArg::SelfRef(x) if x.mutability.is_some() => { },
                _ => err!(x, "First parameter must be &mut self"),
            }
        }
        if method.default.is_some() {
            err!(method.default, "No default implementation allowed");
        }
    }
    // ToDo: doc strings cause soe weird found-a-hash error when used in quotes,
    // so strip  them
    let methods = methods.iter().map(|m| {
        let mut m = m.clone();
        m.attrs.clear(); // ToDo: doc strings cause sytnax error, are transformed to #[""] and that doesn't work in quote!
        m.sig.decl.output = match m.sig.decl.output {
            syn::ReturnType::Type(a, b) => syn::ReturnType::Type(a, b),
            syn::ReturnType::Default => syn::ReturnType::Type(
                    syn::parse(TokenStream::from_str("->").unwrap().into()).unwrap(), Box::new(raw_type!("()")))
        };
        m
    }).collect();
    // we don't mutate an element at the moment, so just clone the vector
    Ok(methods)
}

pub fn gen_iface_methods(methods: &Vec<syn::TraitItemMethod>)
        -> TokenStream {
    let gen = quote! {
        #(#methods)*
    };
    gen.into()
}
