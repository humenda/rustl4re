use l4_sys::l4_msgtag_protocol::{self, *};


/// signed machine word
pub type Mword = isize; // l4_mword_t -> long -> at least 32 bit
/// unsigned machine word
pub type UMword = usize;

/// This macro implements a Rust version of a C enum with nicer names and implementations for
/// BitAnd and BitOr and Not.
macro_rules! enumgenerator {
    ($(#[$global:meta])*
     enum $enum_name:ident {
         $($(#[$docs:meta])*
           $orig:path => $new:tt,
         )+
     }) => {
        #[allow(non_snake_case)]
        $(#[$global])*
        #[derive(Eq, FromPrimitive, PartialEq, ToPrimitive, Debug)]
        pub enum $enum_name {
            $($(#[$docs])*
              $new = $orig as isize,)+
        }
        derive_enum_impl!($enum_name: i32; $($orig => $new),*);
        derive_enum_impl!($enum_name: i64; $($orig => $new),*);
    }
}

#[doc(hidden)]
macro_rules! derive_enum_impl {
    ($enum_name:ident: $type:ty; $($orig:path => $new:ident),*) => {
        impl ::core::ops::BitAnd<$type> for $enum_name {
            type Output=$type;
            fn bitand(self, rhs: $type) -> $type {
                self as $type & rhs
            }
        }
        impl ::core::ops::BitOr<$type> for $enum_name {
            type Output = $type;
            fn bitor(self, rhs: $type) -> $type {
                self as $type | rhs
            }
        }
    }
}

enumgenerator! {
    /// Basic L4 protocols (message tag labels)
    enum Protocol {
        /// Default protocol tag to reply to kernel
        ///
        /// Negative protocol IDs are reserved for kernel-use.
        L4_PROTO_NONE => None,
        /// Make an exception out of a page fault
        l4_msgtag_protocol::L4_PROTO_PF_EXCEPTION => PfException,
        /// IRQ message
        L4_PROTO_IRQ => Irq,
        /// Page fault message
        L4_PROTO_PAGE_FAULT => PageFault,
        /// Preemption message
        L4_PROTO_PREEMPTION => Preemption,
        /// System exception
        L4_PROTO_SYS_EXCEPTION => SysException,
        /// Exception
        L4_PROTO_EXCEPTION => Exception,
        /// Sigma0 protocol
        L4_PROTO_SIGMA0 => Sigma0,
        /// I/O page fault message
        L4_PROTO_IO_PAGE_FAULT => IoPageFault,
        /// Protocol for messages to a a generic kobject
        L4_PROTO_KOBJECT => Kobject,
        /// Protocol for messages to a task object
        L4_PROTO_TASK => Task,
        /// Protocol for messages to a thread object
        L4_PROTO_THREAD => Thread,
        /// Protocol for messages to a log object
        L4_PROTO_LOG => Log,
        /// Protocol for messages to a scheduler object
        L4_PROTO_SCHEDULER => Scheduler,
        /// Protocol for messages to a factory object
        L4_PROTO_FACTORY => Factory,
        /// Protocol for messages to a virtual machine object
        L4_PROTO_VM => Vm,
        /// Protocol for (creating) kernel DMA space objects
        L4_PROTO_DMA_SPACE => DmaSpace,
        /// Protocol for IRQ senders (IRQ -> IPC)
        L4_PROTO_IRQ_SENDER => IrqSender,
        /// Protocol for IRQ mux (IRQ -> n x IRQ)
        L4_PROTO_IRQ_MUX => IrqMux,
        /// Protocol for semaphore objects
        L4_PROTO_SEMAPHORE => Semaphore,
        /// Meta information protocol
        L4_PROTO_META => Meta,
        /// Protocol ID for IO-MMUs
        L4_PROTO_IOMMU => Iommu,
        /// Protocol ID for the ddebugger
        L4_PROTO_DEBUGGER => Debugger,
    }
}
