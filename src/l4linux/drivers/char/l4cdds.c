/*
 * Character driver working on dataspaces, using mem.c as a template.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/ptrace.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include <asm/generic/util.h>

#include <l4/sys/err.h>

MODULE_AUTHOR("Adam Lackorzynski <adam@os.inf.tu-dresden.de");
MODULE_DESCRIPTION("Character driver for L4Re dataspaces");
MODULE_LICENSE("GPL");

enum { NR_DEVS = 4 };

static int  devs_pos;
static int major_num;
module_param(major_num, int, 0);


/*
 * The internal representation of our device.
 */
struct l4cdds_device {
	unsigned long size; // size in bytes of DS
	void *addr;
	l4_cap_idx_t dscap, memcap;
	struct device *dev;
	int read_write;
	char name[20];
};

static struct l4cdds_device device[NR_DEVS];

static int valid_idx(unsigned idx)
{
	return idx < devs_pos && l4_is_valid_cap(device[idx].dscap);
}

static int ds_open(struct inode * inode, struct file * filp)
{
	return capable(CAP_SYS_RAWIO) ? 0 : -EPERM;
}

static loff_t ds_seek(struct file * file, loff_t offset, int orig)
{
	unsigned idx = iminor(file->f_path.dentry->d_inode);

	if (!valid_idx(idx))
		return -ENODEV;

	return fixed_size_llseek(file, offset, orig, device[idx].size);
}

static ssize_t ds_read(struct file * file, char __user * buf,
                       size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned idx = iminor(file->f_path.dentry->d_inode);
	ssize_t read, sz;

	if (!valid_idx(idx))
		return -ENODEV;

	if (p + count > device[idx].size)
		count = device[idx].size - p;

	read = 0;
	while (count > 0) {
		if (-p & (PAGE_SIZE - 1))
			sz = -p & (PAGE_SIZE - 1);
		else
			sz = PAGE_SIZE;

		sz = min_t(unsigned long, sz, count);

		if (copy_to_user(buf, (void *)(device[idx].addr + p), sz))
			return -EFAULT;

		buf += sz;
		p += sz;
		count -= sz;
		read += sz;
	}


	*ppos += read;
	return read;
}

static ssize_t ds_write(struct file * file, const char __user * buf,
                        size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	ssize_t written, sz;
	unsigned long copied;
	unsigned idx = iminor(file->f_path.dentry->d_inode);

	if (!valid_idx(idx))
		return -ENODEV;

	if (p + count > device[idx].size)
		count = device[idx].size - p;

	written = 0;
	while (count > 0) {
		if (-p & (PAGE_SIZE - 1))
			sz = -p & (PAGE_SIZE - 1);
		else
			sz = PAGE_SIZE;

		sz = min_t(unsigned long, sz, count);

		copied = copy_from_user((void *)(device[idx].addr + p), buf, sz);
		if (copied) {
			written += sz - copied;
			if (written)
				break;
			return -EFAULT;
		}

		buf += sz;
		p += sz;
		count -= sz;
		written += sz;
	}

	*ppos += written;
	return written;
}

static int ds_mmap(struct file * file, struct vm_area_struct * vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	unsigned idx = iminor(file->f_path.dentry->d_inode);
	int ret = 0;

	if (!valid_idx(idx))
		return -ENODEV;

	if ((vma->vm_pgoff << PAGE_SHIFT) + size > device[idx].size)
		return -EINVAL;

	if (remap_pfn_range(vma, vma->vm_start,
	                    vma->vm_pgoff
	                     + ((unsigned long)device[idx].addr >> PAGE_SHIFT), size,
	                    vma->vm_page_prot))
		ret = -EAGAIN;

	return ret;
}

static const struct file_operations fops = {
	.open   = ds_open,
	.llseek = ds_seek,
	.read   = ds_read,
	.write  = ds_write,
	.mmap   = ds_mmap,
};

static struct class *l4cdds_class;

static int __init init_one(unsigned idx)
{
	int ret;
	l4re_ds_stats_t stat;

	if (!device_create(l4cdds_class, NULL,
	                   MKDEV(major_num, idx), NULL,
	                   device[idx].name)) {
		pr_err("l4cdds: Device creation failed\n");
		return -ENODEV;
	}

	if (device[idx].read_write) {
		if ((ret = l4x_query_and_get_cow_ds(device[idx].name, "l4cdds",
		                                    &device[idx].memcap,
		                                    &device[idx].dscap,
		                                    &device[idx].addr,
		                                    &stat, 1))) {
			pr_err("Failed to get file: %s(%d)\n",
			       l4sys_errtostr(ret), ret);
			return ret;
		}
	} else {
		if ((ret = l4x_query_and_get_ds(device[idx].name, "l4cdds",
		                                &device[idx].dscap,
		                                &device[idx].addr, &stat))) {
			pr_err("Failed to get file: %s(%d)\n",
			       l4sys_errtostr(ret), ret);
			return ret;
		}
	}

	device[idx].size = stat.size;

	pr_info("l4cdds: Data '%s' size = %lu KB (%lu MB) dev=%d:%d flags=%lx\n",
	        device[idx].name, device[idx].size >> 10, device[idx].size >> 20,
	        major_num, idx, stat.flags);

	return 0;
}

static int __init l4cdds_init(void)
{
	int i;

	if (!devs_pos) {
		pr_info("l4cdds: No name given, not starting.\n");
		return 0;
	}

	major_num = register_chrdev(major_num, "l4cdds", &fops);
	if (major_num <= 0) {
		pr_err("l4cdds: Unable to register chrdev\n");
		return -ENODEV;
	}

	l4cdds_class = class_create(THIS_MODULE, "l4cdds");
	if (IS_ERR(l4cdds_class)) {
		unregister_chrdev(major_num, "l4cdds");
		return PTR_ERR(l4cdds_class);
	}

	for (i = 0; i < devs_pos; ++i)
		init_one(i);

	return 0;
}


static void __exit l4cdds_exit_one(int nr)
{
	if (device[nr].read_write)
		l4x_detach_and_free_cow_ds(device[nr].memcap,
		                           device[nr].dscap,
		                           device[nr].addr);
	else
		l4x_detach_and_free_ds(device[nr].dscap,
		                       device[nr].addr);
}

static void __exit l4cdds_exit(void)
{
	int i;

	for (i = 0; i < devs_pos; i++)
		l4cdds_exit_one(i);

	unregister_chrdev(major_num, "l4cdds");
}

module_init(l4cdds_init);
module_exit(l4cdds_exit);

static int l4cdds_setup(const char *val, const struct kernel_param *kp)
{
	char *p;
	unsigned s;

	if (devs_pos >= NR_DEVS) {
		pr_err("l4cdds: Too many character devices specified\n");
		return 1;
	}
	if ((p = strstr(val, ",rw")))
		device[devs_pos].read_write = 1;
	s = p ? p - val : (sizeof(device[devs_pos].name) - 1);
	strlcpy(device[devs_pos].name, val, s + 1);
	devs_pos++;
	return 0;
}

module_param_call(add, l4cdds_setup, NULL, NULL, 0200);
MODULE_PARM_DESC(add, "Use l4cdds.add=name[,rw] to add another character device, name queried in namespace");
