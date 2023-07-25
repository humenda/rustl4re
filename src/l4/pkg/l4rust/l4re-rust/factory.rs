use crate::sys::l4re_env;
use crate::OwnedCap;

use l4::cap::{Cap, CapIdx, IfaceInit, Interface};
use l4::ipc::MsgTag;
use l4::Result;
use l4_sys::{l4_factory_create};
use l4_sys::l4_msgtag_protocol::{L4_PROTO_IRQ_SENDER, L4_PROTO_DMA_SPACE};

use core::marker::PhantomData;

pub struct Factory<T> {
    cap: CapIdx,
    _typ: PhantomData<T>,
}

impl<T> Interface for Factory<T> {
    #[inline]
    fn raw(&self) -> CapIdx {
        self.cap
    }
}

impl<T> IfaceInit for Factory<T> {
    #[inline]
    fn new(cap: CapIdx) -> Self {
        Self {
            cap,
            _typ: PhantomData,
        }
    }
}

pub struct DefaultFactory;
pub struct IrqSender {
    cap: CapIdx,
}
pub struct DmaSpace {
    cap: CapIdx
}

impl Interface for IrqSender {
    #[inline]
    fn raw(&self) -> CapIdx {
        self.cap
    }
}

impl IfaceInit for IrqSender {
    #[inline]
    fn new(cap: CapIdx) -> Self {
        Self { cap }
    }
}

impl Interface for DmaSpace {
    #[inline]
    fn raw(&self) -> CapIdx {
        self.cap
    }
}

impl IfaceInit for DmaSpace {
    #[inline]
    fn new(cap: CapIdx) -> Self {
        Self { cap }
    }
}


impl Factory<DefaultFactory> {
    pub fn default_factory_from_env() -> Cap<Factory<DefaultFactory>> {
        let env = unsafe { l4re_env().as_ref() }.unwrap();
        Cap::new(env.factory)
    }
}

pub trait FactoryCreate<O> {
    type Args;

    fn create(&self, arg: Self::Args) -> Result<O>;
}

impl FactoryCreate<OwnedCap<IrqSender>> for Factory<DefaultFactory> {
    type Args = ();

    fn create(&self, _arg: ()) -> Result<OwnedCap<IrqSender>> {
        let cap: OwnedCap<IrqSender> = OwnedCap::alloc();
        let tag: MsgTag =
            unsafe { l4_factory_create(self.cap, L4_PROTO_IRQ_SENDER as i32, cap.raw()) }.into();
        tag.result()?;
        Ok(cap)
    }
}

impl FactoryCreate<OwnedCap<DmaSpace>> for Factory<DefaultFactory> {
    type Args = ();

    fn create(&self, _arg: ()) -> Result<OwnedCap<DmaSpace>> {
        let cap: OwnedCap<DmaSpace> = OwnedCap::alloc();
        let tag: MsgTag =
            unsafe { l4_factory_create(self.cap, L4_PROTO_DMA_SPACE as i32, cap.raw()) }.into();
        tag.result()?;
        Ok(cap)
    }
}
