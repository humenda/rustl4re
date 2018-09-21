use l4_sys::l4_msg_regs_t;

/// Bindgen changed its union generation a few times, abstract from it
pub struct MsgMrFake(l4_msg_regs_t);

impl MsgMrFake {
    pub fn new() -> Self {
        MsgMrFake(l4_msg_regs_t::default())
    }

    pub fn get(&self, idx: usize) -> u64 {
        self.0.bindgen_union_field[idx]
    }

    pub unsafe fn mut_ptr(&mut self) -> *mut l4_msg_regs_t {
        &mut self.0 as *mut l4_msg_regs_t
    }
}
