#include <pthread-l4.h>
#include <l4/sys/utcb.h>
#include <l4/sys/types.h>
#include <l4/sys/consts.h>

// custom wrapper to make inlined C functions visible
#include <l4/l4re-rust/ipc.h>

#include <l4/sys/capability>