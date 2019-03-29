//! functions required to implement the iface! macro
use proc_macro2::{Span, TokenStream};
use quote::{quote, ToTokens};
use std::str::FromStr;
use syn::{Attribute,
    braced,
    FnArg,
    Ident,
    parse::{Parse, ParseStream, Result},
    spanned::Spanned,
    Token, Type
};

/// Parsed IPC interface
pub struct Iface {
    pub name: Ident,
    pub protocol: syn::Expr,
    pub opattrs: Vec<syn::Attribute>,
    pub optype: syn::Type,
    pub demand: Demand,
    pub methods: Vec<IfaceMethod>,
}

impl Parse for Iface {
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
                syn::TraitItem::Method(c) => methods.push(
                        IfaceMethod::Unnumbered(c)),
                thing => err!(thing, "Only methods, associated consts and \
                         associated types allowed"),
            }
        }
        // validate attributes and extract relevant info
        let (protocol, opattrs, optype) = Iface::parse_iface_attributes(
                &iface_name, &iface_attrs)?;
        // validate and enumerate methods
        enumerate_iface_methods(&mut methods)?;
        // parse demand
        let demand = count_demand(&methods);
        Ok(Iface {
            name: iface_name, protocol: protocol,
            opattrs: opattrs, optype: optype,
            demand,
            methods: methods
        })
    }
}

// Interfaces can have only a handful meta attributes. One is the PROTOCOL_ID,
// which is mandatory. Optional is the OpType and if it wasn't specified, it'll
// default to i32.
impl Iface {
    pub fn parse_iface_attributes(outer: &syn::Ident, attrs: &Vec<IfaceAttr>)
            -> Result<(syn::Expr, Vec<Attribute>, Type)> {
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
        Ok((protocol, opattrs, optype))
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

pub enum IfaceMethod {
    Unnumbered(syn::TraitItemMethod),
    Numbered(i32, syn::TraitItemMethod)
}

impl IfaceMethod {
    pub fn same_type(&self, other: &Self) -> bool {
        let num = |x: &Self| match x {
            IfaceMethod::Unnumbered(_) => 0,
            IfaceMethod::Numbered(_, _) => 1,
        };
        num(self) == num(other)
    }

    pub fn inner_mut(&mut self) -> &mut syn::TraitItemMethod {
        match self {
            IfaceMethod::Unnumbered(ref mut m) => m,
            IfaceMethod::Numbered(_, ref mut m) => m,
        }
    }

    pub fn inner_ref(&self) -> &syn::TraitItemMethod {
        match self {
            IfaceMethod::Unnumbered(ref m) => m,
            IfaceMethod::Numbered(_, ref m) => m,
        }
    }

    pub fn set_opcode_if_unnumbered(&mut self, opcode: i32) {
        match self {
            IfaceMethod::Unnumbered(m) => *self = IfaceMethod::Numbered(opcode, m.clone()),
            IfaceMethod::Numbered(_, _) => { }
        };
    }
}

impl ToTokens for IfaceMethod {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match self {
            IfaceMethod::Unnumbered(_) => panic!("Trying to generate trait method without opcode"),
            IfaceMethod::Numbered(num, ref m) => {
                let mut ts = proc_macro2::TokenStream::from_str(&format!(
                        "{} => ", num)).unwrap();
                m.to_tokens(&mut ts);
                tokens.extend(ts);
            }
        }
    }
}

pub fn enumerate_iface_methods(methods: &mut Vec<IfaceMethod>)
        -> Result<()> {
    // check whether all numbered or unnumbered
    if methods.len() == 0 {
        err!("Empty interfaces not allowed");
    }
    // check that all methods are either numbered or unnumbered
    if !methods.iter().all(|m| m.same_type(methods.first().unwrap())) {
        err!("Found methods both with and without an opcode");
    }
    
    let mut opcode = 0;
    for method in methods {
        {
            // expected `fn(&mut self, …) -> ret;} (ret is optional)
            let sig = &method.inner_ref().sig;
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
        }
        if method.inner_ref().default.is_some() {
            err!(method.inner_ref().default,
                    "No default implementation allowed");
        }
        // ToDo: doc strings cause soe weird found-a-hash error when used in quotes,
        // so strip  them
        method.inner_mut().attrs.clear();
        method.inner_mut().sig.decl.output = match &method.inner_ref().sig.decl.output {
            syn::ReturnType::Type(a, b) => syn::ReturnType::Type(*a,
                    Box::new(*b.clone())),
            syn::ReturnType::Default => syn::ReturnType::Type(
                    syn::parse(TokenStream::from_str("->").unwrap().into()).unwrap(), Box::new(raw_type!("()")))
        };
        method.set_opcode_if_unnumbered(opcode);
        opcode += 1;
    }
    // we don't mutate an element at the moment, so just clone the vector
    Ok(())
}

pub fn gen_iface(iface: &Iface) -> TokenStream {
    let Iface { name, protocol, opattrs, optype, demand, methods } = iface;
    let caps = demand.caps;
    let gen = quote! {
        l4::iface_back! {
            trait #name {
                const PROTOCOL_ID: i64 = #protocol;
                const CAP_DEMAND: u8 = #caps;
                #(#opattrs)*
                type OpType = #optype;
                #(#methods)*
            }
        }
    };
    gen.into()
}

/// Receive window demand for a receive endpoint
pub struct Demand {
    caps: u8, // more to come
}

pub fn count_demand(methods: &Vec<IfaceMethod>) -> Demand {
    let caps = methods.iter().fold(0u8, |count, method| {
        let sig = &method.inner_ref().sig;
        // add old count + n capabilities
        count + sig.decl.inputs.iter().filter(|arg| {
            // only count "proper" types, no self references, etc.
            let ty = match *arg {
                FnArg::Ignored(ty) => ty,
                FnArg::Captured(argty) => &argty.ty,
                _ => return false, // do not count
            };
            // only accept "paths" such as foo::bar
            let ty = match ty {
                syn::Type::Path(p) => p,
                _ => return false,
            };
            // get baz of foo::bar::baz
            let segment = match ty.path.segments.last() {
                None => return false, // empty type?
                Some(segment) => match segment {
                    syn::punctuated::Pair::Punctuated(t, _) => t,
                    syn::punctuated::Pair::End(t) => t,
                },
            };
            // finaly, get actual type string
            match segment.ident.to_string().as_str() {
                "Cap" => true, // count + 1
                _ => false,
            }
        }).count() as u8
    });
    Demand { caps }
}
