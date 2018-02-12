// vim: expandtab
#ifndef __L4RE_HELPER_H
#define __L4RE_HELPER_H

#include "pub_core_debuglog.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_aspacemgr.h"

enum {
    DEBUG_OFF = 0,
    DEBUG_ON  = 1,
};

enum
{
    VRMcap_valgrind = 0xABCD,
    VRMcap_client   = 0xFEDC,
};

// ---------------------------------------------------------------------
//              Constants
// ---------------------------------------------------------------------
enum Consts {
    MAX_LOCAL_RM_CAPS = 1024,
    MAX_VG_CAPS       = 1024,
    VG_UTCB_OFFSET    = 2048, // 4 UTCBs
};


extern const Bool dbg_pf;
extern const Bool dbg_elf;
extern const Bool dbg_rm;
extern const Bool dbg_ds;
extern const Bool dbg_vcap;

static const unsigned VG_DEBUG_L4RE_FILES = DEBUG_ON;

extern int vcap_running;

/*
 * BD: I regularly need this function, so let's make it a
 *     global utility.
 */
static void __attribute__((unused)) l4re_hexdump(unsigned char *buf, unsigned len)
{
    unsigned i = 0;

    for ( ; i < len; ++i)
        VG_(printf)("%02x ", *(buf + i));
    VG_(printf)("\n");
}


static inline char const * __attribute__((unused)) vcap_segknd_str(int kind)
{
    switch(kind) {
        case SkFree:  return "free";
        case SkAnonC: return "anon client";
        case SkAnonV: return "anon VG";
        case SkFileC: return "file client";
        case SkFileV: return "file VG";
        case SkShmC:  return "shared mem client";
        case SkResvn: return "reservation";
    }

    return "<invalid?>";
}

int VG_(x86_l4re_REDIR_FOR_dl_open)(char const *, int, int);
int VG_(x86_l4re_REDIR_FOR_dl_close)(int);
void *VG_(x86_l4re_REDIR_FOR_dl_mmap)(l4_addr_t addr, int len, int prot, int flags, int fd, int offset);

extern Addr _vfs_p;
void init_vfs_proxy(void);

/* 
 * VRM - aspacemem interactions
 */
void vrm_segment_notify(Addr, Addr, l4_cap_idx_t, unsigned, unsigned);
void vrm_segment_fixup(NSegment *);
void vrm_update_segptr(NSegment *, l4_cap_idx_t, unsigned, unsigned);

/*
 * VRM-internal tracking of fd-name mappings.
 */
void vrm_register_filename(int fd, Char *name);
void vrm_unregister_filename(int fd);
Char *vrm_get_filename(int fd);

/*
 * Tell a possibly interested tool abouut the UTCB area. This needs
 * to be done once the tool is running.
 */
void vrm_track_utcb_area(void);

#endif
