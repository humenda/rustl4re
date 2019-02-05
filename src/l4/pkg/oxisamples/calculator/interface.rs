/// Interface definition for the simple calculator
///
/// Similar to the C++ implementation, the Rust interface definition can be
/// shared among client and server. This is the shared interface.
/// It looks very similar to a Rust trait definition, by purpose. The only
/// difference is that `&mut self` is required for the self reference and that
/// the `PROTOCOL_ID` needs to be specified.
///
/// This interface is 100 % binary compatible to the L4Re `clntsrv` example, as
/// long as the order of methods is preserved.
iface! {
    trait Calculator {
        const PROTOCOL_ID: i64 = 0x44;
        fn sub(&mut self, a: u32, b: u32) -> i32;
        fn neg(&mut self, a: u32) -> i32;
    }
}
