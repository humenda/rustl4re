use l4_sys::l4_msg_regs_t;
use l4::utcb::UtcbMr;

pub struct UtcbMrFake {
    mem: Vec<u64>, // typical msg register count on x86_64
    mr: UtcbMr,
}

impl UtcbMrFake {
    pub fn new() -> Self {
        let mut v = vec![0u64; 63];
        let ptr = v.as_mut_slice().as_mut_ptr();
        UtcbMrFake {
            mem: v,
            mr: unsafe { UtcbMr::from_mr(ptr as *mut _) }
        }
    }

    pub fn get(&self, idx: usize) -> u64 {
        self.mem.get(idx).unwrap().clone()
    }

    pub fn set(&mut self, idx: usize, val: u64) {
        self.mem[idx] = val;
    }

    pub unsafe fn mut_ptr(&mut self) -> *mut l4_msg_regs_t {
        self.mem.as_mut_slice().as_mut_ptr() as *mut l4_msg_regs_t
    }

    pub fn msg(&mut self) -> &mut UtcbMr {
        &mut self.mr
    }
}

impl std::fmt::Debug for UtcbMrFake {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        self.mem.fmt(f)
    }
}

