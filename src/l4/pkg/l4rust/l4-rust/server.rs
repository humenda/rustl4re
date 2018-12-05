trait Demand {
    /// number of capability receive buffers.
    const CAPS: u8 = 0;
    ///< FLAGS, such as the need for timeouts (TBD).
    const FLAGS: u8 = 0;
    /// number of MEMory receive buffers.
    const MEM: u8 = 0;
    /// number of I/O PORTS
    const PORTS: u8 = 0;

    fn no_demand() -> bool {
        Self::CAPS == 0 && Self::FLAGS == 0 && Self::MEM == 0 && Self::PORTS == 0
    }
}

trait Interface {
    const PROTOCOL: i32 = 0;
}
