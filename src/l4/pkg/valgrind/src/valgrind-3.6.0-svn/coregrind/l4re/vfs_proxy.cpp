/*
 * This file is part of the Valgrind port to L4Re.
 *
 * (c) 2009-2010 Aaron Pohle <apohle@os.inf.tu-dresden.de>,
 *               Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 */
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/sys/kdebug.h>
#include <cstring>

__BEGIN_DECLS
#include "pub_core_basics.h"
#include "pub_tool_libcbase.h"
__END_DECLS

// l4/pkg/l4re_vfs/lib/src/vfs.cc
namespace Vfs_config {

using L4Re::Util::cap_alloc;
using ::memcpy;

inline
L4::Cap<L4Re::Mem_alloc> allocator()
{ return L4Re::Env::env()->mem_alloc(); }

inline int 
load_module(char const *)
{ return -1; }

}

#include <l4/l4re_vfs/impl/ns_fs_impl.h>
#include <l4/l4re_vfs/impl/ro_file_impl.h>
#include <l4/l4re_vfs/impl/fd_store_impl.h>
#include <l4/l4re_vfs/impl/vcon_stream_impl.h>
#include <l4/l4re_vfs/impl/vfs_api_impl.h>
#include <l4/l4re_vfs/impl/vfs_impl.h>

class Vfs_proxy : public Vfs
{
    public:

    Vfs_proxy() : Vfs()
    {
        outstring("\033[32mVfs_proxy\033[0m\n");
    }
};

static Vfs_proxy *_vfs_proxy;

EXTERN_C Addr _vfs_p   = (Addr)&_vfs_proxy;
EXTERN_C Addr _l4revfs = 0;

EXTERN_C void init_vfs_proxy()
{
    _vfs_proxy = new Vfs_proxy();
}
