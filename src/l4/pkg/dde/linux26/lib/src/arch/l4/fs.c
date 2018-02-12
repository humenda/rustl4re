/*
 * This file is part of DDE/Linux2.6.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "local.h"

#include <linux/fs.h>
#include <linux/backing-dev.h>
#include <linux/mount.h>

/*
 * Some subsystems, such as the blockdev layer, implement their data
 * hierarchy as a pseudo file system. To not incorporate the complete
 * Linux VFS implementation, we cut this down to an own one only for
 * pseudo file systems.
 */
static LIST_HEAD(dde_vfs_mounts);

#define MAX_RA_PAGES 1

void default_unplug_io_fn(struct backing_dev_info *bdi, struct page* p)
{
}

struct backing_dev_info default_backing_dev_info = {
	.ra_pages     = MAX_RA_PAGES,
	.state        = 0,
	.capabilities = BDI_CAP_MAP_COPY,
	.unplug_io_fn = default_unplug_io_fn,
};

int seq_puts(struct seq_file *m, const char *f)
{
	WARN_UNIMPL;
	return 0;
}

int seq_printf(struct seq_file *m, const char *f, ...)
{
	WARN_UNIMPL;
	return 0;
}

int generic_writepages(struct address_space *mapping,
                       struct writeback_control *wbc)
{
	WARN_UNIMPL;
	return 0;
}

/**************************************
 * Filemap stuff                      *
 **************************************/
int test_set_page_writeback(struct page *page)
{
	WARN_UNIMPL;
	return 0;
}

void do_invalidatepage(struct page *page, unsigned long offset)
{
	WARN_UNIMPL;
}

int redirty_page_for_writepage(struct writeback_control *wbc, struct page *page)
{
	WARN_UNIMPL;
	return 0;
}

struct ddekit_thread {
        long pthread;
        void *data;
        void *stack;
        void *sleep_cv;
        const char *name;
};
struct ddekit_thread *ddekit_thread_myself(void);

static struct vfsmount *dde_kern_mount(struct file_system_type *type,
			 	       int flags, const char *name, void *data)
{
	struct list_head *pos, *head;
	int error;
        ddekit_printf ("WAH %s:%s:%d %s\n", __FILE__, __FUNCTION__, __LINE__, ddekit_thread_myself()->name);

	head = &dde_vfs_mounts;
	__list_for_each(pos, head) {
		struct vfsmount *mnt = list_entry(pos, struct vfsmount, next);
		if (strcmp(name, mnt->name) == 0) {
			printk("FS type %s already mounted!?\n", name);
			BUG();
			return NULL;
		}
	}

	struct vfsmount *m = kzalloc(sizeof(*m), GFP_KERNEL);
	m->fs_type = type;
	m->name = kmalloc(strlen(name) + 1, GFP_KERNEL);
	memcpy(m->name, name, strlen(name) + 1);
        ddekit_printf ("WAH %s:%s:%d %s %p\n", __FILE__, __FUNCTION__, __LINE__, ddekit_thread_myself()->name, type);
	error = type->get_sb(type, flags, name, data, m);
	BUG_ON(error);

	list_add_tail(&m->next, &dde_vfs_mounts);

	return m;
}

struct vfsmount *kern_mount_data(struct file_system_type *type, void *data)
{
  ddekit_printf ("WAH %s:%s:%d %s %p\n", __FILE__, __FUNCTION__, __LINE__, ddekit_thread_myself()->name, type);
	return dde_kern_mount(type, 0, type->name, data);
}
