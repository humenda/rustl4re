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
use syn::{self, Data, Fields};
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


/// Transform a struct into an L4 server
///
/// IPC servers are required to fulfill certain memory constraints and implement 
/// a few unsafe traits. This macro makes sure that these are met. More
/// information can be found at the
/// [l4::ipc::types::Callable trait](../l4/ipc/types/trait.Callable.html),
/// [l4::ipc::types::Dispatch](../l4/ipc/types/trait.Dispatch.html), and
/// [l4::ipc::types::Demand](../l4/ipc/types/trait.Demand.html) traits.
/// 
/// The macro takes two arguments in parenthesis: the trait to base the IPC
/// server on and an optional buffer size to allocate an internal buffer on the
/// stack. This is explained at
/// [l4::ipc::server::TypedBuffer](../l4/ipc/server/trait.TypedBuffer.html).
/// Specifying no buffer assumes a size of 0.
///
/// # Examples
///
/// ```norun
/// #[l4_server(MyGreatIpcIface)] // <- no buffer
/// struct Simple;
/// #[l4_server(MyGreatIpcIface, buffer=256)] // buffer of 256 bytes
/// struct MoreComplex {
///     just_an_example: u32,
/// }
/// ```
#[proc_macro_attribute]
pub fn l4_server(macro_attrs: TokenStream, item: TokenStream) -> TokenStream {
    if macro_attrs.is_empty() {
        proc_err!("IPC trait missing, use l4_server(TRAITNAME)");
    }
    let opts = proc_err!(clntsrv::parse_server_meta(parse_macro_input!(
            macro_attrs as syn::AttributeArgs)));

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
                                             &opts),
        Fields::Unnamed(_) => proc_err!("Only named structs or unnamed structs \
                can be turned into an IPC server."),
        Fields::Unit => clntsrv::gen_server_struct(name, ast.attrs, ast.vis,
                                             ast.generics, Fields::Unit,
                                             &opts),
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
    let mut fs = proc_err!(File::create(opts.output_file).map_err(|e|
            syn::Error::new(proc_macro2::Span::call_site(), e.to_string())));
    proc_err!(fs.write_all(cpp.as_bytes()).map_err(|e|
            syn::Error::new(proc_macro2::Span::call_site(), e.to_string())));
    TokenStream::new()
}
