// a few constants are not auto-imported by the platform-independent part of
// some dependency of ipc.h, so include it manually
//#include <amd64/l4/sys/consts.h>
//#include <l4/sys/consts.h>
//
//// the actual  IPC header
//#include <l4/sys/utcb.h>
//#include <l4/sys/ipc.h>
//// todo comes from <l4sys/include/__timeout.h>, but that's not imported by rust
//#define L4_IPC_TIMEOUT_NEVER ((l4_timeout_s){0})            // never timeout
//
#include <pthread-l4.h>
// custom wrapper to make inlined C functions visible
#include <l4/libl4ipc-rust-wrapper/lib.h>
