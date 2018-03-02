// a few constants are not auto-imported by the platform-independent part of
// some dependency of ipc.h, so include it manually
//#include <amd64/l4/sys/consts.h>
#include <l4/sys/consts.h>

// the actual  IPC header
#include <l4/sys/ipc.h>
