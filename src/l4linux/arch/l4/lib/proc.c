/*
 * /proc/l4, do not enhance, use debugfs instead
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/list.h>

#include <asm/api/macros.h>
#include <asm/l4lxapi/thread.h>

#include <asm/generic/hybrid.h>
#include <asm/generic/task.h>

#define DIRNAME "l4"

struct proc_dir_entry *l4_proc_dir;

EXPORT_SYMBOL(l4_proc_dir);

/* ------------------------------------------- */

static void *hybrid_seq_start(struct seq_file *m, loff_t *pos)
{
	return (*pos == 0) ? pos : NULL;
}

static void *hybrid_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;
	return NULL;
}

static void hybrid_seq_stop(struct seq_file *m, void *v)
{
	/* Nothing to do */
}

static struct seq_operations hybrid_seq_ops = {
	.start = hybrid_seq_start,
	.next  = hybrid_seq_next,
	.stop  = hybrid_seq_stop,
	.show  = l4x_hybrid_seq_show,
};

static int l4x_hybrid_open(struct inode *inode, struct file *filp)
{
	return seq_open(filp, &hybrid_seq_ops);
}

static const struct proc_ops l4x_hybrid_proc_ops = {
	.proc_open    = l4x_hybrid_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = seq_release,
};

/* ------------------------------------------- */

static int __init l4x_proc_init(void)
{
	int r = 0;

	/* create directory */
	if ((l4_proc_dir = proc_mkdir(DIRNAME, NULL)) == NULL)
		return -ENOMEM;

	/* seq file ones */
	if (!proc_create("hybrids", 0, l4_proc_dir, &l4x_hybrid_proc_ops))
		pr_err("l4x: Failed to create proc direcotry\n");

	return r;
}

/* automatically start this */
subsys_initcall(l4x_proc_init);
