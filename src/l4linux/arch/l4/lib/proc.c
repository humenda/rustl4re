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

static struct file_operations l4x_hybrid_fops = {
	.open    = l4x_hybrid_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

/* ------------------------------------------- */

static void l4x_create_seq_entry(char *name, mode_t mode, struct file_operations *f)
{
	if (!proc_create(name, mode, l4_proc_dir, f))
		pr_err("l4x: Failed to create proc direcotry\n");
}

static int __init l4x_proc_init(void)
{
	int r = 0;

	/* create directory */
	if ((l4_proc_dir = proc_mkdir(DIRNAME, NULL)) == NULL)
		return -ENOMEM;

	/* seq file ones */
	l4x_create_seq_entry("hybrids",  0, &l4x_hybrid_fops);

	return r;
}

/* automatically start this */
subsys_initcall(l4x_proc_init);
