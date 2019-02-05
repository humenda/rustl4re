#![recursion_limit="128"]
#![feature(proc_macro_diagnostic)]
extern crate proc_macro;
extern crate proc_macro2;

use std::str::FromStr;

use crate::proc_macro::TokenStream;
use quote::quote;
use syn::{self, Attribute, Data, Fields, Generics, Ident, Lit,
        NestedMeta, Meta, Visibility};

mod iface;

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
            let name_val: syn::MetaNameValue = match syn::parse(macro_attrs)
                    .expect("Broken attribute specification") {
                Meta::NameValue(nv) => nv,
                _ => panic!("You must specify demand = <num>"),
            };
            // check for correct key within parenthesis
            if name_val.ident != "demand" {
                panic!("Unrecognised macro attribute, try \"demand\"");
            }
            // only accept numbers
            match name_val.lit {
                Lit::Int(l) => l.value() as u32,
                _ => panic!("`demand` must be a positive integer"),
            }
        }
    };

    let ast: syn::DeriveInput = syn::parse(item).expect("Unable to parse struct definition.");
    let structdef = match ast.data {
        Data::Struct(ds) => ds,
        _ => panic!("This attribute can only be used on structs"),
    };
    let name = ast.ident;
    // we can only insert a new member into a named struct or unit structs.
    // If we'd insert into a tuple struct, this would move the index of all
    // existing members, invalidating any user's code.
    match structdef.fields {
        Fields::Named(_) => gen_server_struct(name, ast.attrs, ast.vis,
                                             ast.generics, structdef.fields,
                                             demand),
        Fields::Unnamed(_) => panic!("Only named structs or unnamed structs \
                can be turned into an IPC server."),
        Fields::Unit => gen_server_struct(name, ast.attrs, ast.vis,
                                             ast.generics, Fields::Unit,
                                             demand),
    }
}

fn gen_server_struct(name: proc_macro2::Ident, attrs: Vec<Attribute>,
                    vis: Visibility, generics: Generics, fields: Fields,
                    demand: u32)
                    -> proc_macro::TokenStream {
    // duplicate names and types of fields, because quote! moves them
    let initialiser_names: Vec<_> = fields.iter().map(|f| f.ident.clone()).collect();
    let arg_names: Vec<_> = fields.iter().map(|f| f.ident.clone()).collect();
    let arg_types: Vec<_> = fields.iter().map(|f| f.ty.clone()).collect();

    let gen = quote! {
        #[repr(C)]
        #(#attrs)*
        #vis struct #name #generics {
            __dispatch_ptr: ::l4::ipc::Callback,
            __cap: crate::l4::cap::CapIdx,
            #(#fields),*
        }

        impl #name {
            /// Create a new #name instance
            ///
            /// The first argument is the IPC gate that this server is bound to.
            /// For example: `#name::new(my_cap, #(#arg_names),*)`.
            fn new(cap: l4::cap::CapIdx, #(#arg_names: #arg_types),*) -> #name {
                #name {
                    __dispatch_ptr: crate::l4::ipc::server_impl_callback::<#name>,
                    __cap: cap,
                    #(
                        #initialiser_names
                    ),*
                }
            }
        }
        impl crate::l4::ipc::types::Demand for #name {
            const BUFFER_DEMAND: u32 = #demand;
        }
        unsafe impl crate::l4::ipc::types::Callable for #name { }
        impl crate::l4::ipc::types::Dispatch for #name {
            fn dispatch(&mut self, tag: crate::l4::ipc::MsgTag,
                        mr: &mut l4::utcb::UtcbMr,
                        bufs: &mut l4::ipc::BufferAccess)
                    -> crate::l4::error::Result<crate::l4::ipc::MsgTag> {
                // op_dispatch is auto-generated by the iface! macro
                self.op_dispatch(tag, mr, bufs)
            }
        }
        impl l4::cap::Interface for #name {
            unsafe fn cap(&self) -> l4::cap::CapIdx {
                self.__cap
            }
        }
        impl l4::ipc::CapProviderAccess for #name { }
    };
    gen.into()
}

#[proc_macro_attribute]
pub fn l4_client(macro_attrs: TokenStream, item: TokenStream) -> TokenStream {
    // parse trait name for IPC communication
    if macro_attrs.is_empty() {
        panic!("At least the IPC trait to derive must be specified");
    }
    let mut trait_name = None;
    let mut demand: u32 = 0;
    parse_client_meta(syn::parse(macro_attrs)
                .expect("Please specify at least a valid IPC trait name"),
        &mut trait_name, &mut demand);
    let trait_name = trait_name.expect("No IPC trait specified");

    let ast: syn::DeriveInput = syn::parse(item).expect("Unable to parse struct definition.");
    let structdef = match ast.data {
        Data::Struct(ds) => ds,
        _ => panic!("This attribute can only be used on structs"),
    };
    let name = ast.ident;
    // only unit structs are accepted, because cap types generally don't have members (in fact, the
    // only valid member is inserted)
    match structdef.fields {
        Fields::Unnamed(_) | Fields::Named(_) => panic!("Only unit structs (no data \
                fields) can be turned into an IPC client."),
        Fields::Unit => gen_client_struct(name, ast.attrs, ast.vis,
                                             ast.generics, trait_name, demand)
    }
}

fn parse_client_meta(meta: syn::Meta, name: &mut Option<Ident>,
                      demand: &mut u32) {
    // recursively parse meta arguments to macro
    match meta {
        Meta::Word(tn) => *name = Some(tn),
        Meta::NameValue(nv) => {
            if nv.ident != "demand" {
                panic!("Only `demand = NUM` allowed");
            }
            *demand = match nv.lit {
                Lit::Int(i) => i.value() as u32,
                _ => panic!("Demand can only be a positive integer number"),
            };
        },
        Meta::List(ml) => {
            for thing in ml.nested.into_iter() {
                match thing {
                    NestedMeta::Meta(nm) => parse_client_meta(nm, name, demand),
                    _ => panic!("Invalid attribute: only trait name and \
                            demand are allowed"),
                }
            }
        },
    }

}

fn gen_client_struct(name: proc_macro2::Ident, attrs: Vec<Attribute>,
                    vis: Visibility, generics: Generics, trait_name: Ident,
                    demand: u32)
                -> proc_macro::TokenStream {
    let slot_type: syn::Type = match demand {
        0 => syn::parse(TokenStream::from_str("l4::ipc::Bufferless").unwrap()).unwrap(),
        _ => syn::parse(TokenStream::from_str("l4::ipc::BufferManager").unwrap()).unwrap()
    };
    let gen = quote! {
        #(#attrs)*
        #vis struct #name #generics {
            __cap: crate::l4::cap::CapIdx,
            __slots: #slot_type,
        }

        impl crate::l4::cap::Interface for #name {
            unsafe fn cap(&self) -> crate::l4::cap::CapIdx {
                self.__cap
            }
        }
        impl crate::l4::cap::IfaceInit for #name {
            fn new(c: crate::l4::cap::CapIdx) -> Self {
                #name {
                    __cap: c,
                    __slots: <#slot_type as l4::ipc::CapProvider>::new(),
                }
            }
        }
        impl #trait_name for #name { }

        impl l4::ipc::CapProviderAccess for #name {
            unsafe fn access_buffers(&mut self) -> l4::ipc::BufferAccess {
                use l4::ipc::CapProvider;
                self.__slots.access_buffers()
            }
        }
    };
    gen.into()
}

macro_rules! proc_try {
    ($match:expr) => {
        match $match {
            Ok(x) => x,
            Err(_) => return proc_macro2::TokenStream::new().into(),
        }
    }
}

// ToDo: docs for this macro and **parse** docs for trait; auto-derive demand
#[proc_macro]
pub fn iface(tokens: TokenStream) -> TokenStream {
    let iface::RawIface {
        iface_name, iface_attrs, methods
    } = syn::parse_macro_input!(tokens as iface::RawIface);
    let parser_results = proc_try!(iface::parse_iface_attributes(
            iface_name.clone(), &iface_attrs));
    let parsed_iface_attrs = iface::gen_iface_attrs(parser_results);
    let methods = iface::enumerate_iface_methods(&methods).unwrap();
    if methods.is_empty() {
        return methods.into();
    }
    let gen = quote! {
        l4::iface_enumerate! {
            trait #iface_name {
                #parsed_iface_attrs
                #methods
            }
        }
    };
    gen.into()
}
