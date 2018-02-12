/*
 * Kernel part of the xf86 driver
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/bitops.h>

#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <asm/generic/l4fb.h>

#include <l4/l4con/l4con_ev.h>
#include <l4/l4con/l4con-client.h>
#include <l4/env/errno.h>
#include <l4/sys/kdebug.h>

extern struct proc_dir_entry *l4_proc_dir;

typedef struct {
	struct pid *pid;
	unsigned long active;
} client_t;

static atomic_t              xf86used = ATOMIC_INIT(0);
static int                   xf86arg;
static struct fasync_struct *fasync;
static struct task_struct   *sig_proc;
static client_t              client_list[8];

static client_t* client_find(struct pid *pid)
{
	int i;
	for (i=0; i<sizeof(client_list)/sizeof(client_list[0]); i++)
		if (client_list[i].pid == pid)
			/* found */
			return client_list + i;

	/* not found */
	return NULL;
}

static void client_activate(struct pid *pid)
{
	client_t *c;

	if ((c = client_find(pid))) {
		/* already in list */
		if (!test_and_set_bit(0, &c->active))
			atomic_inc(&xf86used);
		l4fb_refresh_status_set(L4FB_REFRESH_DISABLED);
		return;
	}
	/* still no entry */
	if ((c = client_find(0))) {
		c->pid = get_pid(pid);
		set_bit(0, &c->active);
		atomic_inc(&xf86used);
		l4fb_refresh_status_set(L4FB_REFRESH_DISABLED);
		return;
	}
	/* Hmm ... */
	enter_kdebug("client_list too small");
}

static client_t* client_deactivate(struct pid *pid)
{
	client_t *c;

	if ((c = client_find(pid))) {
		/* already in list */
		if (test_and_clear_bit(0, &c->active))
			if (!atomic_dec_and_test(&xf86used))
				l4fb_refresh_status_set(L4FB_REFRESH_ENABLED);
		return c;
	}
	/* Spurious client. This should never happen since a client has to open
	 * /proc/l4/l4fb_xf86if before it is able to access it ... */
	for (;;)
		enter_kdebug("spurious client");
}

static void client_kill(struct pid *pid)
{
	client_t *client = client_deactivate(pid);
	client->pid = NULL;
	put_pid(pid);
}

static int l4fb_xf86if_handle_redraw_event(void)
{
	if (fasync && atomic_read(&xf86used)) {
		xf86arg = 1; /* map FB, redraw X screen */
		kill_fasync(&fasync, SIGIO, 0);
		send_sig(SIGUSR1, sig_proc, 1);
		return 1;
	}
	/* no -> let comh thread do it */
	return 0;
}

static int l4fb_xf86if_handle_background_event(void)
{
	if (fasync && atomic_read(&xf86used)) {
		xf86arg = 2;
		kill_fasync(&fasync, SIGIO, 0);
	}
	return 0;
}

static int l4fb_xf86if_input_hook_function(long type, long code)
{
	/* don't handle it ourself if X isn't in the foreground */
	if (atomic_read(&xf86used) && type == EV_CON) {
		switch (code) {
			case EV_CON_REDRAW:
				l4fb_xf86if_handle_redraw_event();
				return 1;
			case EV_CON_BACKGROUND:
				l4fb_xf86if_handle_background_event();
				return 1;
		}
	}
	return 0;
}

static int l4fb_xf86if_f_open(struct inode *i, struct file *f)
{
	l4fb_input_event_hook_register(l4fb_xf86if_input_hook_function);
	client_activate(task_pid(current));
	sig_proc = current;
	return 0;
}

static int l4fb_xf86if_f_release(struct inode *i, struct file *f)
{
	client_kill(f->f_owner.pid);
	return 0;
}

static int l4fb_xf86if_f_ioctl(struct inode *i, struct file *f,
                               unsigned int cmd, unsigned long arg)
{
	struct __attribute__((packed)) {
		l4_threadid_t    x_id;
		l4_threadid_t    vc_id;
		l4dm_dataspace_t ds_id;
	} ctu;
	l4_threadid_t t;
	int res;
	DICE_DECLARE_ENV(_env);

	switch (_IOC_NR(cmd)) {
		case 1:
			if (copy_from_user(&ctu, (void *)arg, sizeof(ctu)))
				return -EFAULT;
			ctu.vc_id = l4fb_con_vc_id_get();
			ctu.ds_id = l4fb_con_ds_id_get();
			l4dm_share(&ctu.ds_id, ctu.x_id, L4DM_RW);
			if ((res = con_vc_share_call(&ctu.vc_id, &ctu.x_id, &_env))
			    || DICE_HAS_EXCEPTION(&_env))
				printk("%s: Error sharing console: %s(%d)\n",
				       __func__, l4env_errstr(res), res);
			if (copy_to_user((void *)arg, &ctu, sizeof(ctu)))
				return -EFAULT;
			break;
		case 2:
			t = l4fb_con_con_id_get();
			if (copy_to_user((void *)arg, &t, sizeof(t)))
				return -EFAULT;
			break;
		case 3:
			client_deactivate(f->f_owner.pid);
			break;
		case 4:
			client_activate(f->f_owner.pid);
			sig_proc = current;
			break;
		case 5:
			if (copy_to_user((void *)arg, &xf86arg, sizeof(xf86arg)))
				return -EFAULT;
			xf86arg = 0;
			break;
		default:
			printk("%s: Unknown command 0x%x\n", __func__, cmd);
			return -EINVAL;
	};
	return 0;
}

static int l4fb_xf86if_f_async(int fd, struct file *filp, int on)
{
	struct fasync_struct *fa, **fp;
	unsigned long flags;

	for (fp = &fasync; (fa = *fp) != NULL; fp = &fa->fa_next)
		if (fa->fa_file == filp)
			break;

	if (on) {
		if (fa) {
			fa->fa_fd = fd;
			return 0;
		}

		fa = kmalloc(sizeof(struct fasync_struct), GFP_KERNEL);
		if (!fa)
			return -ENOMEM;

		fa->magic = FASYNC_MAGIC;
		fa->fa_file = filp;
		fa->fa_fd = fd;
		local_irq_save(flags);
		fa->fa_next = fasync;
		fasync = fa;
		local_irq_restore(flags);
		return 1;
	}
	if (!fa)
		return 0;

	local_irq_save(flags);
	*fp = fa->fa_next;
	local_irq_restore(flags);

	kfree(fa);
	return 1;
}

static struct file_operations l4fb_xf86if_fops = {
	.owner   = THIS_MODULE,
	.open    = l4fb_xf86if_f_open,
	.ioctl   = l4fb_xf86if_f_ioctl,
	.release = l4fb_xf86if_f_release,
	.fasync  = l4fb_xf86if_f_async,
};

int __init l4fb_xf86if_init(void)
{
	struct proc_dir_entry *d;
	d = create_proc_entry("l4fb_xf86if", 0, l4_proc_dir);
	if (d)
		d->proc_fops = &l4fb_xf86if_fops;
	return 0;
}
module_init(l4fb_xf86if_init);

void __exit l4fb_xf86if_exit(void)
{
	remove_proc_entry("l4fb_xf86if", l4_proc_dir);
}

MODULE_AUTHOR("Frank Mehnert, Adam Lackorzynski <{fm3,adam}@os.inf.tu-dresden.de>");
MODULE_DESCRIPTION("Support for L4 con");
MODULE_LICENSE("GPL");
