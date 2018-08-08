fn shutdown() {
    unsafe {
        let pfc = l4re_sys::l4re_env_get_cap("pfc");
        if l4_sys::l4_is_invalid_cap(pfc) {
            panic!("Invalid platform control capability, insufficient task rights?");
        }
        let tag = l4_sys::l4_platform_ctl_system_shutdown(pfc, 0);
        if l4_sys::l4_msgtag_has_error(tag) {
            panic!("Error while shutting down: {}",
                    l4_sys::l4_ipc_error(tag, l4_utcb()));
        }
    }
}

#[no_mangle]
fn main() {
    println!("bye");
    shutdown();
}
