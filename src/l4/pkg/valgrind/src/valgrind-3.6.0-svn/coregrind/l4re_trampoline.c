// vim: expandtab
#include <valgrind.h>
#include <unistd.h>
#include <l4/sys/kdebug.h>

#include "pub_core_basics.h"
#include "pub_tool_libcbase.h"
#include "pub_core_vki.h"
#include "pub_core_libcsetjmp.h"
#include "pub_core_threadstate.h"

#define L4RE_TRAP_MAGIC \
    "push %%eax\t\n" \
    "rol $0x42, %%eax\t\n" \
    "ror $0x42, %%eax\t\n" \
    "pop %%eax\t\n"

#define L4RE_TRAP_PARAM_CONSTRAINT "a"

#define L4REDIR_TRAP(ptr) \
    do { asm volatile ( \
            L4RE_TRAP_MAGIC \
          : \
          : L4RE_TRAP_PARAM_CONSTRAINT ((ptr)) \
          : "memory" \
    ); } while (0)

#define VG_TRAMPOLINE_FUNC __attribute__((section(".l4re_vg_trampoline")))


VG_TRAMPOLINE_FUNC void VG_(x86_l4re_REDIR_FOR_syscall_page) (void) 
{
/*    int res; 
    VALGRIND_DO_CLIENT_REQUEST(res, 0, VG_USERREQ__GET_L4RE_SYSCALL, 0,
                               0, 0, 0, 0);*/
    // the client can't execute commands from the syscall page directly, so we
    // use a redirection to this function and provide a mirrored syscall page
    // here TODO mirror syscall page through copying from real syscall page -
    // is this possible??
    asm (
            "int    $0x30\n"
    );
}


VG_TRAMPOLINE_FUNC int
VG_(x86_l4re_REDIR_FOR_dl_open)(char const *path, int flags, int mode)
{
    OrigFn f;
    VALGRIND_GET_ORIG_FN(f);

    L4RedirState s;
    int ret = 0;

    CALL_FN_W_WWW(ret, f, path, flags, mode);

    s.redir_type = LIBC_OPEN;
    s.retval     = ret;
    s.open.path  = path;
    s.open.flags = flags;

    L4REDIR_TRAP(&s);

    return ret;
}


VG_TRAMPOLINE_FUNC int
VG_(x86_l4re_REDIR_FOR_dl_close)(int fd)
{
    OrigFn f;
    VALGRIND_GET_ORIG_FN(f);

    L4RedirState s;
    int ret = 0;

    CALL_FN_W_W(ret, f, fd);

    s.redir_type = LIBC_CLOSE;
    s.retval     = ret;
    s.close.fd   = fd;

    L4REDIR_TRAP(&s);

    return ret;
}


VG_TRAMPOLINE_FUNC void *
VG_(x86_l4re_REDIR_FOR_dl_mmap)(l4_addr_t addr, int len, int prot, int flags, int fd, int offset)
{
    OrigFn f;
    VALGRIND_GET_ORIG_FN(f);

    L4RedirState s;
    int ret = 0;

    CALL_FN_W_6W(ret, f, addr, len, prot, flags, fd, offset);

    s.redir_type  = LIBC_MMAP;
    s.retval      = ret;
    s.mmap.addr   = addr;
    s.mmap.size   = VG_PGROUNDUP(len);
    s.mmap.prot   = prot;
    s.mmap.flags  = flags;
    s.mmap.fd     = fd;
    s.mmap.offset = offset;

    L4REDIR_TRAP(&s);

#undef LOW_BIT
    return (void*)ret;
}
