extern crate proc_macro;

use crate::proc_macro::TokenStream;
use quote::quote;
use syn::{self, Data};

#[proc_macro_attribute]
pub fn derive_callable(_a: TokenStream, item: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(item).expect("Unable to parse struct definition.");
    let structdef = match ast.data {
        Data::Struct(ds) => ds,
        _ => panic!("This attribute can only be used on structs"),
    };
    let name = ast.ident;
    let fields: Vec<_> = structdef.fields.iter().collect();
    let field_names: Vec<_> = fields.iter().map(|f| f.ident.clone()).collect();
    let initialiser_names: Vec<_> = fields.iter().map(|f| f.ident.clone()).collect();
    let arg_names: Vec<_> = fields.iter().map(|f| f.ident.clone()).collect();
    let field_types: Vec<_> = fields.iter().map(|f| f.ty.clone()).collect();
    let arg_types: Vec<_> = fields.iter().map(|f| f.ty.clone()).collect();
    let gen = quote! {
        #[repr(C)]
        struct #name {
            __dispatch_ptr: ::l4::ipc::Callback,
            #(#field_names: #field_types),*
        }

        impl #name {
            fn new(#(#arg_names: #arg_types),*) -> #name {
                #name {
                    __dispatch_ptr: crate::l4::ipc::server_impl_callback::<#name>,
                    #(
                        #initialiser_names
                    ),*
                }
            }
        }
    };
    gen.into()
}
