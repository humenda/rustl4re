use l4_sys::*;

use error::{Error, Result};

/// signed machine word
pub type Mword = isize; // l4_mword_t -> long -> at least 32 bit
/// unsigned machine word
pub type UMword = usize;

/// This macro helps to interface with the C-version of the enum easily, while providing a Rustic
/// interface and serialisation / deserialisation capabilities.
macro_rules! enumgenerator {
    ($fieldname:ident; i32 => 
     $(#[$global:meta])*
     fields:
     $($(#[$docs:meta])*
       $orig:ident => $new:tt,
     )+) => {
        #[allow(non_snake_case)]
        #[repr(i32)]
        $(#[$global])*
        pub enum $fieldname {
            $($(#[$docs])*
                $new,)+
        }
        impl $fieldname {
            /// Initialise from raw number
            pub fn from(i: i32) -> Result<$fieldname> {
                match i {
                    $($orig => Ok($fieldname::$new),)+
                    n => Err(Error::InvalidArg(format!("Invalid value for initialisation of $fieldname: {}", n))),
                }
            }

            pub fn as_raw(&self) -> i32 {
                match *self {
                    $($fieldname::$new => ($orig as i32),)*
                }
            }
        }
    };

    // match u32 case; note: we cannot catch the $type:ty, because repr() is evaluated *before*
    // this macro expansion and then $type is a literal
    ($fieldname:ident; u32 => 
     $(#[$global:meta])*
     fields:
     $($(#[$docs:meta])*
       $orig:ident => $new:tt,
     )+) => {
        #[allow(non_snake_case)]
        #[repr(u32)]
        $(#[$global])*
        pub enum $fieldname {
            $($(#[$docs])*
                $new,)+
        }
        impl $fieldname {
            /// Initialise from raw number
            pub fn from(i: u32) -> $fieldname {
                match i {
                    $($orig => $fieldname::$new,)+
                }
            }

            pub fn as_raw(&self) -> u32 {
                match *self {
                    $($fieldname::$new => ($orig as u32),)*
                }
            }
        }
    }
}

enumgenerator! (Protocol; i32 =>
    /// Basic L4 protocols (message tag labels)

fields:
    /// Default protocol tag to reply to kernel
    ///
    /// Negative protocol IDs are reserved for kernel-use.
    L4_PROTO_NONE => None,
    /// Make an exception out of a page fault
    L4_PROTO_PF_EXCEPTION => PfException,
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
);

