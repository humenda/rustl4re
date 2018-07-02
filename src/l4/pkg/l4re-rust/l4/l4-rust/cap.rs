use std::ops::Deref;

use error::{Error, Result};

pub struct Cap<T>(T);

impl<T> Deref for Cap<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.0
    }
}

impl<T> Cap<T> {
    pub fn alloc(&mut self) -> Result<()> {
        Ok(())
    }

    pub fn is_valid() -> bool {
    }

    pub fn is_invalid() -> bool {
    }
}
