use l4::cap::{Cap, IfaceInit};
use sys::l4re_env_get_cap;

pub fn get_cap<T: IfaceInit>(name: &str) -> Option<Cap<T>> {
    l4re_env_get_cap(name).map(|cap_idx| Cap::<T>::new(cap_idx))
}
