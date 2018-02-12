#include <linux/debugfs.h>
#include <asm/generic/stats.h>

struct dentry *l4x_debugfs_dir;

u32 l4x_dbg_feature_1;

static int __init dbg_setup(char *s)
{
	l4x_dbg_feature_1 = simple_strtoul(s, NULL, 0);
	return 1;
}

__setup("l4x_dbg_feature_1=", dbg_setup);

#ifdef CONFIG_L4_DEBUG_STATS
extern struct l4x_debug_stats l4x_debug_stats_data;

#define L4X_DEBUGFS_STAT(x) \
	{ #x, offsetof(struct l4x_debug_stats, l4x_debug_stats_##x), \
	  sizeof(l4x_debug_stats_data.l4x_debug_stats_##x) }

struct l4x_stats_debugfs_item l4x_debugfs_entries[] = {
	L4X_DEBUGFS_STAT(suspend),
	L4X_DEBUGFS_STAT(pagefault),
	L4X_DEBUGFS_STAT(exceptions),
	L4X_DEBUGFS_STAT(pagefault_but_in_PTs),
	L4X_DEBUGFS_STAT(pagefault_write),
};

static int stats_init(void)
{
	int err, i;
	void *datap = &l4x_debug_stats_data;
	struct dentry *statdir;
	const mode_t mode = S_IRUGO | S_IWUSR | S_IWGRP;

	statdir = debugfs_create_dir("stats", l4x_debugfs_dir);
	if ((err = PTR_ERR_OR_ZERO(statdir))) {
		printk(KERN_ERR "Failed to create \"l4lx/stats\" debugfs dir:"
		                " %d\n", err);
		return err;
	}

	for (i = 0; i < ARRAY_SIZE(l4x_debugfs_entries); ++i) {
		struct l4x_stats_debugfs_item *e = &l4x_debugfs_entries[i];
		switch (e->width) {
			case 1:
				e->dentry = debugfs_create_u8(e->name, mode,
				                              statdir,
				                              e->offset + datap);
				break;
			case 2:
				e->dentry = debugfs_create_u16(e->name, mode,
				                               statdir,
				                               e->offset + datap);
				break;
			case 4:
				e->dentry = debugfs_create_u32(e->name, mode,
				                               statdir,
				                               e->offset + datap);
				break;
			case 8:
				e->dentry = debugfs_create_u64(e->name, mode,
				                               statdir,
				                               e->offset + datap);
				break;
			default:
				printk(KERN_ERR "Unable to create debugfs entry \"%s\", " \
				                "width %d not supported\n",
				                e->name, e->width);
		}
	}
	return 0;
}

#endif

static int __init l4x_debugfs_init(void)
{
	int err;

	l4x_debugfs_dir = debugfs_create_dir("l4x", NULL);
	if ((err = PTR_ERR_OR_ZERO(l4x_debugfs_dir))) {
		printk(KERN_ERR "Failed to create \"l4x\" debugfs directory:"
		                " %d\n", err);
		return err;
	}

	debugfs_create_u32("l4x_dbg_feature_1", S_IWUSR | S_IRUSR,
	                   l4x_debugfs_dir, &l4x_dbg_feature_1);

#ifdef CONFIG_L4_DEBUG_STATS
	err = stats_init();
	if (err)
		return err;
#endif
	return 0;
}

subsys_initcall(l4x_debugfs_init);
