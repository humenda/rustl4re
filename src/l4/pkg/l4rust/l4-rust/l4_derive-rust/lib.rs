#![recursion_limit="128"]
extern crate proc_macro;
extern crate proc_macro2;
extern crate lazy_static;

#[macro_use]
mod macros;
mod clntsrv;
mod export;
mod iface;

use proc_macro::TokenStream;
use syn::{self, Data, Fields, Lit};
use syn::spanned::Spanned;
use syn::parse_macro_input;
use std::fs::File;
use std::io::Write;

macro_rules! proc_err {
    // an error string spanning the whole macro invocation
    ($lit:literal) => {
        return syn::Error::new(proc_macro2::Span::call_site(), $lit)
                .to_compile_error().into();
    };

    // use given span to emit error message with location info
    ($span:expr, $lit:literal) => {
        return syn::Error::new($span.span(), $lit)
                .to_compile_error().into();
    };

    // same as err!: use given syn::Error to emit the error message
    ($match:expr) => {
        match $match {
            Ok(x) => x,
            Err(e) => return e.to_compile_error().into(),
        }
    };
}


/// Derive important IPC traits for an IPC interface
///
/// IPC interfaces need to adhere to a few criteria before they can be
/// registered with the server loop. If you are impatient, use `#[l4_server]`
/// on your struct if you are  writing a server without any flex page exchange
/// and `#[l4_server(demand = XYZ)]` for the maximum number of capability/flex
/// page slots for an IPC call of your interface.
///
/// For the patient: an interface needs to implement the `Dispatch` and
/// `Callable`trait. `Callable` is a bit special, since it requires
/// modifications to the struct itself and this macro makes sure that the user
/// does not need to bother about this.  
/// Each server interface may receive capabilities / flex pages, for which it
/// needs to prepare receive slots, so that the kernel can map it into the own
/// object space. The maximum over all IPC functions of an interface is called
/// the `demand` and can be specified as `#[l4_server(demand = NUM)]`.
#[proc_macro_attribute]
pub fn l4_server(macro_attrs: TokenStream, item: TokenStream) -> TokenStream {
    // parse the demand from the parenthesis after the macro name or assume 0
    let demand: u32 = match macro_attrs.is_empty() {
        true => 0,
        false => {
            // only accept `name = value`or abort otherwise
            let name_val = parse_macro_input!(macro_attrs
                    as syn::MetaNameValue);
            if name_val.ident != "demand" {
                proc_err!("Unrecognised macro attribute, try \"demand\"");
            }
            // only accept numbers
            match name_val.lit {
                Lit::Int(l) => l.value() as u32,
                _ => proc_err!(name_val, "`demand` must be a positive integer"),
            }
        }
    };

    let ast: syn::DeriveInput = syn::parse(item).expect("Unable to parse struct definition.");
    let structdef = match ast.data {
        Data::Struct(ds) => ds,
        _ => proc_err!("Attribute can only be used on structs"),
    };
    let name = ast.ident;
    // we can only insert a new member into a named struct or unit structs.
    // If we'd insert into a tuple struct, this would move the index of all
    // existing members, invalidating any user's code.
    match structdef.fields {
        Fields::Named(_) => clntsrv::gen_server_struct(name, ast.attrs, ast.vis,
                                             ast.generics, structdef.fields,
                                             demand),
        Fields::Unnamed(_) => proc_err!("Only named structs or unnamed structs \
                can be turned into an IPC server."),
        Fields::Unit => clntsrv::gen_server_struct(name, ast.attrs, ast.vis,
                                             ast.generics, Fields::Unit,
                                             demand),
    }
}

/// Derive a l4 client implementation
///
/// This attribute prepares a unit (empty) struct to be used as client-side IPC
/// interface. The trait name of the IPC interface to be derived is mandatory,
/// the demand (number of maximum capabilities to be received) is optional and
/// defaults to 0.
///
/// Example:
///
/// ```norun
/// iface! {
///     trait Calculation { … }
/// }
///
/// #[l4_client(Calculation, 0)]
/// struct Calculator;
/// ```
#[proc_macro_attribute]
pub fn l4_client(macro_attrs: TokenStream, item: TokenStream) -> TokenStream {
    // parse trait name for IPC communication
    if macro_attrs.is_empty() {
        proc_err!("No IPC interface specified");
    }
    let parsed_attrs = parse_macro_input!(macro_attrs as syn::AttributeArgs);
    let (trait_name, demand) = proc_err!(clntsrv::parse_client_meta(parsed_attrs));

    let ast: syn::DeriveInput = syn::parse(item).expect("Unable to parse struct definition.");
    let structdef = match ast.data {
        Data::Struct(ds) => ds,
        _ => proc_err!("Attribute can only be used on structs"),
    };
    let name = ast.ident;
    // only unit structs are accepted, because cap types generally don't have members (in fact, the
    // only valid member is inserted)
    match structdef.fields {
        Fields::Unnamed(_) | Fields::Named(_) => proc_err!("Only unit structs (no data \
                fields) can be turned into an IPC client."),
        Fields::Unit => clntsrv::gen_client_struct(name, ast.attrs, ast.vis,
                                             ast.generics, trait_name, demand)
    }
}

/// L4 IPC interface definition
///
/// In its simplest form, IPC on L4 is a series of wait, send, and reply
/// operations (plus their joined versions). Message registers are filled with
/// words and items (the latter being references to kernel objects which are
/// treated different). It sounds very simple, but due to their simplicity,
/// interactions can get very complex. IPC interfaces allow the developer to
/// define an interface which is very close to a normal Rust interface, with the
/// benefit the a lot of the server code and almost all client code is
/// auto-derived. The user does not need to deal with the low-level details such
/// as string, vector or capability deserialisation and can focus on the
/// protocol itself.
///
/// Example:
///
/// ```
/// use l4_derive::iface;
///
/// iface! {
///     trait Example {
///         const PROTOCOL_ID = 98765432;
///         fn signal(&mut self);
///         fn sum(&mut self, x: Vec<i32>) -> i64;
///         fn greet(&mut self, how: String) -> String;
///     }
/// }
/// ```
///
/// The differ areences to a normal trait:
///
/// -   The `PROTOCOL_ID` is mandatory and should ideally name a system-unique
///     protocol identifier to discriminate the communication from other IPC
///     interfaces.
/// -   Each function needs to have `&mut self` as first argument.
/// -   Functions return either `()` (if nothing was specified) or the specified
///     type; servers implementing the trait *must* wrap the specified return
///     type within `l4::error::Result`.
/// -   It is possible to specify `type OpType = <TYPE>` and it defaults to an
///     i32. The opcode is used to identify the requested operation and is set
///     to i32 for C++ compatibility.
///
/// The [`l4_client`](fn.l4_client.html) and [`l4_server`](fn.l4_server.html)
/// explain the usage of an interface.
#[proc_macro]
pub fn iface(tokens: TokenStream) -> TokenStream {
    let parsed_iface = parse_macro_input!(tokens as iface::Iface);
    iface::gen_iface(&parsed_iface).into()
}

#[proc_macro]
pub fn iface_export(input: TokenStream) -> TokenStream {
    // parse proc macro input (used as conversion options)
    let opts: export::ExportOptions = proc_err!(syn::parse(input));
    // read Iface struct
    let iface = proc_err!(export::parse_external_iface(&opts.input_file));
    let cpp = proc_err!(export::gen_cpp_interface(&iface, &opts));
    let fname = match opts.output_file {
        None => {
            let mut s = opts.input_file.clone();
            if s.ends_with(".rs") {
                s.truncate(s.len() - 2);
            }
            s.push_str("h");
            s
        },
        Some(x) => x,
    };
    let mut fs = proc_err!(File::create(fname).map_err(|e|
            syn::Error::new(proc_macro2::Span::call_site(), e.to_string())));
    proc_err!(fs.write_all(cpp.as_bytes()).map_err(|e|
            syn::Error::new(proc_macro2::Span::call_site(), e.to_string())));
    TokenStream::new()
}
