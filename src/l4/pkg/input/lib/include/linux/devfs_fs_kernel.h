#ifndef _LINUX_DEVFS_FS_KERNEL_H
#define _LINUX_DEVFS_FS_KERNEL_H

static inline int devfs_mk_dir(const char *fmt, ...)
{
	return 0;
}
static inline void devfs_remove(const char *fmt, ...)
{
}

#endif

