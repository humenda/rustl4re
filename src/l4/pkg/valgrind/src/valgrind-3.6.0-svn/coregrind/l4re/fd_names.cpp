/*
 * This file is part of the Valgrind port to L4Re.
 *
 * (c) 2009-2010 Aaron Pohle <apohle@os.inf.tu-dresden.de>,
 *               Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 */

#include <l4/sys/compiler.h>
__BEGIN_DECLS
#include "pub_core_basics.h"
#include "pub_l4re.h"
#include "pub_l4re_consts.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcfile.h"
#include "pub_core_libcprint.h"
#include "l4re_helper.h"
__END_DECLS

// max number of fd->name mappings
static const int _num_filenames = 25;
// storage for mappings
Char _filenames[_num_filenames][50] = { { 0 } };


void vrm_register_filename(int fd, Char *name)
{
    if (fd < 0)
        return;

    if (fd > _num_filenames) {
        VG_(printf)("fd out of range: %d\n",fd);
        return;
    }

    VG_(strncpy)(_filenames[fd], name, 50);
}


void vrm_unregister_filename(int fd)
{
    if (fd < 0)
        return;

    if (fd > _num_filenames) {
        VG_(printf)("fd out of range: %d\n",fd);
        return;
    }

    VG_(memset)(_filenames[fd], 0, 50);
}


Char *vrm_get_filename(int fd)
{
    if (fd < 0 || fd > _num_filenames) {
        VG_(printf)("fd out of range: %d\n",fd);
        return 0;
    }

    return _filenames[fd];
}
