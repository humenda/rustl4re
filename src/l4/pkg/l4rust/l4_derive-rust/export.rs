use proc_macro::TokenStream;
use quote::ToTokens;
use std::fs;
use std::{
    collections::{HashMap, HashSet},
    str::FromStr,
    string::ToString,
};
use syn::parse::Parse;
use syn::spanned::Spanned;
use syn::{
    parse::ParseStream, punctuated::Punctuated, Expr, FnArg, Lit, LitStr, Result, Token,
    TraitItemMethod,
};

use super::iface::Iface;

lazy_static::lazy_static! {
    /// Mappings from Rust to C types. Generics are stripped and treated separately,
    /// so `Option<u32>` will be looked up as `Option` and `u32`.
    static ref RUST2CPP_TYPE: HashMap<&'static str, &'static str> = {
        let mut map = HashMap::new();
        map.insert("char", "char");
        map.insert("i8", "char");
        map.insert("u8", "unsigned char");
        map.insert("i16", "l4_int16_t");
        map.insert("u16", "l4_uint16_t");
        map.insert("i32", "l4_int32_t");
        map.insert("u32", "l4_uint32_t");
        map.insert("i64", "l4_int64_t");
        map.insert("u64", "l4_uint64_t");
        map.insert("f32", "float");
        map.insert("f64", "double");
        map.insert("usize", "l4_umword_t");
        map.insert("isize", "l4_mword_t");
        map.insert("bool", "bool");
        map.insert("Option", "Opt");
        map.insert("Cap", "Cap");
        map.insert("String", "String<>"); // needs <>, otherwise strange template errors
        map.insert("str", "String<>");
        map
    };

    static ref CPPTYPE2NAMESPACE:
            HashMap<&'static str, (&'static str, Option<&'static str>)> = {
        let mut map = HashMap::new();
        // cpp type: (namespace, includefile)
        map.insert("Opt", ("L4::Ipc::Opt", Some("l4/sys/cxx/ipc_types")));
        map.insert("Cap", ("L4::Ipc::Cap", Some("l4/sys/capability")));
        map.insert("String", ("L4::Ipc::String", Some("l4/sys/cxx/ipc_string")));
        // l4re
        map.insert("Dataspace", ("L4Re::Dataspace", Some("l4/re/dataspace")));
        map
    };

    // certain types use different output types (AKA return typees)
    static ref RUST2CPP_OUTPUT:
            HashMap<&'static str, &'static str> = {
        let mut map = HashMap::new();
        // Rust type: Cpp output type (return type)
        map.insert("String", "String<char>");
        map.insert("str", "String<char>");
        map
    };
}

/// Mapping from Rust name to (C++ Namespace, C++ type name); namespace may be empty
type TypeMappings = HashMap<String, (String, String)>;

/// Options to adjust the exported C++ interface
pub struct ExportOptions {
    /// file containing the interface definition using `iface!`
    pub input_file: String,
    /// Output file (optional), will use `input_file` with the `.h` extension
    pub output_file: String,
    /// Optional specification of an alternate C++ interface name to adhere to
    /// other naming conventions
    pub name: Option<String>,
    /// Overwrite protocol specification (e.g. if a constant instead of a number
    /// was used)
    pub protocol: Option<u64>,
    /// additional types: Rust type → (cpp name space, cpp type)
    pub type_mappings: TypeMappings,
    /// Additional mappings from C++ types to their include files
    pub includes: HashSet<String>,
}

impl syn::parse::Parse for ExportOptions {
    fn parse(input: syn::parse::ParseStream) -> Result<Self> {
        let err = |msg| syn::Error::new(proc_macro2::Span::call_site(), msg);
        let err_sp = |sp: proc_macro2::Span, msg| syn::Error::new(sp, msg);
        let mut lit_opts = std::collections::HashMap::new();
        let mut ty_map = HashMap::new();
        let mut includes = HashSet::new();
        while !input.is_empty() {
            let key: syn::Ident = input.parse()?;
            let _: Token![:] = input.parse()?;
            match &*key.to_string() {
                // parse "foo"  => ("bar", "baz")
                "add_types" => ty_map.extend(Self::parse_ty_mappings(input)?),
                "add_includes" => includes.extend(Self::parse_includes(input)?),
                _ => {
                    let val: Lit = input.parse()?;
                    lit_opts.insert(key.to_string(), val);
                }
            };
            if !input.is_empty() {
                let _: Token![,] = input.parse()?;
            }
        }
        let input_file = lit_opts
            .get("input_file")
            .ok_or(err("No input file specified"))
            .map(|l| match l {
                Lit::Str(x) => Ok(x.value()),
                _ => Err(err_sp(l.span(), "Invalid input_file")),
            })??;
        Ok(ExportOptions {
            input_file: input_file.clone(),
            output_file: lit_opts
                .get("output_file")
                .map(|l| match l {
                    Lit::Str(x) => Ok(x.value()),
                    _ => Err(err_sp(l.span(), "Only string literals allowed")),
                })
                .transpose()?
                .or(Some(input_file.replace(".rs", ".h")))
                .unwrap(),
            name: lit_opts
                .get("name")
                .map(|l| match l {
                    Lit::Str(x) => Ok(x.value()),
                    _ => return Err(err_sp(l.span(), "Invalid input_file")),
                })
                .transpose()?,
            protocol: lit_opts
                .get("protocol")
                .map(|l| match l {
                    Lit::Int(l) => Ok(l.value()),
                    _ => Err(err_sp(l.span(), "Only (positive) integers allowed")),
                })
                .transpose()?,
            type_mappings: ty_map,
            includes: includes,
        })
    }
}

impl ExportOptions {
    /// parse additional `"rust" => ("namespace", "C++ type"), …}` mappings
    fn parse_ty_mappings(input: ParseStream) -> Result<TypeMappings> {
        let content;
        let _brace = syn::braced!(content in input);
        let mut mappings: TypeMappings = HashMap::new();
        // parse expressions as `"foo" => ("bar", "baz"), ...`
        while !content.is_empty() {
            let key: LitStr = content.parse()?;
            let _arrow: Token![=>] = content.parse()?;
            let tpl_content;
            let _ = syn::parenthesized!(tpl_content in content);
            let tuple_expr: Punctuated<Expr, Token![,]> =
                tpl_content.parse_terminated(Expr::parse)?;
            // slightly complex expression to collect ("", "") into a Vec
            let v: Vec<Result<String>> = tuple_expr
                .iter()
                .map(|elem| {
                    let l = ifletelse!(elem => syn::Expr::Lit; "Expected literal string");
                    let l = ifletelse!(l.lit => Lit::Str; "Expected a literal string");
                    Ok((*l.value()).to_owned())
                })
                .collect();
            match v.len() {
                2 => mappings.insert(key.value(), (v[0].clone()?, v[1].clone()?)),
                _ => err!(key, "Required tuple of two literal strings as value"),
            };
        }
        Ok(mappings)
    }

    fn parse_includes(input: ParseStream) -> Result<HashSet<String>> {
        let content;
        let _bracket = syn::bracketed!(content in input);
        let mut includes = HashSet::new();
        // parse ["path1", "path2", …]
        while !content.is_empty() {
            let include: LitStr = content.parse()?;
            if !content.is_empty() {
                let _comma: Token![,] = content.parse()?;
            }
            includes.insert(include.value());
        }
        Ok(includes)
    }
}

// fuse of try! and a .map_err
macro_rules! map_err {
    ($spanned:expr, $expression:expr) => {
        $expression.map_err(|e| syn::Error::new($spanned.span(), e.to_string()))
    };

    ($spanned:expr, $expression:expr, $msg:literal) => {
        $expression.ok_or(syn::Error::new($spanned.span(), $msg))
    };
}

pub fn parse_external_iface(filename: &str) -> Result<Iface> {
    let contents = map_err!(filename, fs::read_to_string(filename))?;
    let start = map_err!(
        filename,
        contents.find("iface!"),
        "No iface! macro invocation found"
    )?;
    // find { after iface!
    let start = start
        + map_err!(
            filename,
            contents[start..].find('{'),
            "Interface definition without opening brace '{'"
        )?
        + 1;
    let mut braces = 1;
    let mut end = None;
    for (index, ch) in contents[start + 1..contents.len() - 1].chars().enumerate() {
        match ch {
            '{' => braces += 1,
            '}' => braces -= 1,
            _ => (),
        };
        if braces == 0 {
            end = Some(start + index);
            break;
        }
    }
    let end = map_err!(filename, end, "Unterminated IPC interface definition")?;
    let iface: Iface = syn::parse(
        TokenStream::from_str(&contents[start..end])
            .map_err(|e| syn::Error::new(filename.span(), format!("{:?}", e)))?,
    )?;
    Ok(iface)
}

// translate type from Rust to C++
// Returned is the fully translated type name (including generics); required
// namespace_usg and includes will be added to the given sets.; empty Strings are allowed for unit
// structs which cannot be translated to C++ types, since they don't reserve any space
fn translate_type(
    additional: &TypeMappings,
    ty: &syn::Type,
    namespace_usg: &mut HashSet<String>,
    includes: &mut HashSet<String>,
    is_output: bool,
) -> Result<String> {
    let ty = match ty {
        syn::Type::Path(p) => p,
        syn::Type::Tuple(tp) if tp.elems.is_empty() => return Ok(String::new()),
        syn::Type::Reference(r) => {
            return translate_type(additional, &*r.elem, namespace_usg, includes, is_output)
        }
        _ => {
            let mut ts = proc_macro2::TokenStream::new();
            ty.to_tokens(&mut ts);
            let ty = ts.to_string();
            // unit type?
            match ty
                .chars()
                .filter(|&c| !c.is_whitespace())
                .collect::<String>()
                .as_str()
            {
                "()" => return Ok(String::new()),
                n => err!(format!("Unrecognised type expression: {}", n)),
            };
        }
    };

    // only accept Type::Path(…)
    // use only last segment (e.g. l4::cap::Cap, use only Cap)
    let segment = match ty.path.segments.last() {
        None => err!(ty, "Invalid type specification"),
        Some(segment) => match segment {
            syn::punctuated::Pair::Punctuated(t, _) => t,
            syn::punctuated::Pair::End(t) => t,
        },
    };

    let main = segment.ident.to_string();
    let mut translated = additional
        .get(main.as_str())
        .map(|x| x.1.clone())
        .or(RUST2CPP_TYPE.get(main.as_str()).map(|e| e.to_string()))
        .unwrap_or(main.clone());
    if is_output {
        // if output differs from input, use this, otherwise use normal C++ type
        translated = String::from(
            *RUST2CPP_OUTPUT
                .get(&main.as_str())
                .unwrap_or(&translated.as_str()),
        );
    }
    let decl =
        CPPTYPE2NAMESPACE
            .get(main.as_str())
            .map(|e| *e)
            .or(match additional.get(main.as_str()) {
                Some((usg, _)) => Some((usg, None)),
                None => None, // if additional type mapping used, omit include path
            });
    if let Some((usg, incl)) = decl {
        namespace_usg.insert(format!("{}", usg));
        if let Some(incl) = incl {
            includes.insert(format!("{}", incl));
        }
    }

    // transform generics / lifetimes, if any
    match &segment.arguments {
        syn::PathArguments::None => (),
        syn::PathArguments::AngleBracketed(args) => {
            let mut transformed_generics = String::new();
            for arg in &args.args {
                match &arg {
                    syn::GenericArgument::Lifetime(_) => (), // ignore those
                    syn::GenericArgument::Type(t) => transformed_generics.push_str(
                        &translate_type(additional, &t, namespace_usg, includes, is_output)?,
                    ),
                    _ => err!(args, "Invalid type parameter"),
                }
                transformed_generics.push_str(", ");
            }
            if transformed_generics.len() > 0 {
                transformed_generics.truncate(transformed_generics.len() - 2); // strip ", "
                translated.push_str(&format!("<{}>", transformed_generics));
            }
        }
        _ => err!(format!(
            "Only types with generic types supported, got: {:?}",
            segment.arguments
        )),
    };
    match translated == "()" {
        true => Ok(String::new()), // C++ doesn't have zero-sized types
        false => Ok(translated),
    }
}

pub fn gen_cpp_interface(iface: &Iface, opts: &ExportOptions) -> Result<String> {
    // fails on non-integer protocol literal from iface!
    let protocol = match opts.protocol {
        Some(x) => x,
        None => {
            let lit_enum = ifletelse!(iface.protocol => syn::Expr::Lit;
                    "No protocol specified and no protocol literal in interface definition.");
            let lit = ifletelse!(lit_enum.lit => Lit::Int;
                    "Protocol literal is not an integer");
            lit.value()
        }
    };
    // use custom interface name or default
    let name = opts.name.clone().unwrap_or(iface.name.to_string());

    let demand = match iface.demand.caps {
        0 => String::new(),
        n => format!(", L4::Type_info::Demand_t<{}>", n),
    };

    // construct list with namespaces, includes and type mappings
    let mut namespace_usg = HashSet::new(); // gathered below
                                            // default includes
    let mut includes = HashSet::new();
    includes.insert(String::from("l4/sys/capability"));
    includes.insert(String::from("l4/sys/cxx/ipc_iface"));
    includes.extend(opts.includes.clone());

    let mut cpp_iface = format!(
        "struct {0}: L4::Kobject_t<{0}, \
            L4::Kobject, {1}{2}> {{\n",
        name, protocol, demand
    );
    // variable name literally required by the C++ framework
    let mut rpcs = Vec::new();
    for method in (*iface.methods).iter() {
        rpcs.push(method.inner_ref().sig.ident.to_string());
        cpp_iface.push_str(&gen_cpp_rpc_method(
            method.inner_ref(),
            &opts.type_mappings,
            &mut namespace_usg,
            &mut includes,
        )?);
    }
    // add rpcs typedef
    cpp_iface.push_str("  typedef L4::Typeid::Rpcs<");
    rpcs.iter()
        .for_each(|n| cpp_iface.push_str(&format!("{}_t, ", n)));
    cpp_iface.truncate(cpp_iface.len() - 2); // strip last ", "
    cpp_iface.push_str("> Rpcs;\n};\n");

    let mut preamble = String::new();
    let mut includes = includes.iter().collect::<Vec<&String>>();
    includes.sort();
    includes
        .iter()
        .for_each(|x| preamble.push_str(&format!("#include <{}>\n", x)));
    let mut namespace_usg = namespace_usg.iter().collect::<Vec<&String>>();
    namespace_usg.sort();
    if namespace_usg.len() > 0 {
        preamble.push_str("\n");
    }

    namespace_usg
        .iter()
        .for_each(|x| preamble.push_str(&format!("using {};\n", x)));
    match !preamble.chars().all(|c| c.is_whitespace()) {
        true => Ok(format!("{}\n{}", preamble, cpp_iface)),
        false => Ok(cpp_iface),
    }
}

fn gen_cpp_rpc_method(
    method: &TraitItemMethod,
    ty_mappings: &TypeMappings,
    namespace_usg: &mut HashSet<String>,
    includes: &mut HashSet<String>,
) -> Result<String> {
    let mut used_vars = Vec::new(); // for arguments having no name in Rust
    fn new_var(used: &mut Vec<String>) -> String {
        let x = (97u8..).find(|n| !used.contains(&format!("{}", *n as char)));
        let x = (x.unwrap() as char).to_string();
        used.push(x.clone());
        x
    }
    let mut cpp = format!("  L4_INLINE_RPC(int, {}, (", method.sig.ident.to_string());

    for (idx, fnarg) in method.sig.decl.inputs.iter().enumerate() {
        let (mut name, argtype) = match fnarg {
            FnArg::SelfRef(_) | FnArg::SelfValue(_) => continue,
            FnArg::Inferred(_pat) => unimplemented!(),
            FnArg::Ignored(ty) => (String::new(), ty),
            FnArg::Captured(argty) => match &argty.pat {
                syn::Pat::Ident(i) => (i.ident.to_string(), &argty.ty),
                _ => {
                    let s = format!("Invalid argument type {:?}", fnarg);
                    err!(argty, s);
                }
            },
        };
        match name.is_empty() {
            true => name = new_var(&mut used_vars),
            false => used_vars.push(name.clone()),
        };
        // serialise the argument type to a string
        let argtype = translate_type(&ty_mappings, argtype, namespace_usg, includes, false)?;
        if idx > 1 {
            // skip self and no comma before first argument
            cpp.push_str(", ");
        }
        cpp.push_str(&format!("{} {}", argtype, name));
    }
    // return type
    if let syn::ReturnType::Type(_, ty) = &method.sig.decl.output {
        let ty = translate_type(&ty_mappings, ty, namespace_usg, includes, true)?;
        if !ty.is_empty() {
            // no return type
            cpp.push_str(&format!(", {}& {}", ty, new_var(&mut used_vars)));
        }
    }
    cpp.push_str("));\n");
    Ok(cpp)
}
