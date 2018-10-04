The L4Re snapshot usually comes with a demonstration client/server
implementation, providing a simple calculator for two operations.  This
directory contains the rust-side implementations and three configuration files:

-   `rcalc-client.conf`: for using the Rust client with the C++ server
-   `rcalc-server`: for using the Rust (calculator) server with the C++ client
-   `rcalc-client-server`: for using both sides in Rust

Note that you need to compile the `$PKGDIR/examples/clntsrv` example first.

The `src/l4/conf/modules.list` entry for the rcalc-client.conf could look like
this:

```
entry ox_rcalc-client
kernel fiasco -serial_esc
roottask moe rom/rcalc-client.conf
module l4re
module ned
module rcalc-client.conf
module ox_rcalc-client
module ex_clntsrv-server
```
