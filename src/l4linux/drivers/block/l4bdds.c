/*
 * Block driver working on dataspaces.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/blkdev.h>
#include <linux/dax.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/kernel.h>
#include <linux/pfn_t.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <asm/api/macros.h>
#include <asm/generic/util.h>

#include <l4/sys/task.h>
#include <l4/sys/err.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/dataspace.h>
#include <l4/re/c/rm.h>

MODULE_AUTHOR("Adam Lackorzynski <adam@os.inf.tu-dresden.de");
MODULE_DESCRIPTION("Block driver for L4Re dataspaces");
MODULE_LICENSE("GPL");

enum { NR_DEVS = 4 };

static int devs_pos;
static int major_num = 0;        /* kernel chooses */
module_param(major_num, int, 0);

enum {
	KERNEL_SECTOR_SHIFT = 9,
};

/*
 * The internal representation of our device.
 */
struct l4bdds_device {
	unsigned long ds_size; /* in bytes */
	spinlock_t lock;
	struct gendisk *gd;
	l4_cap_idx_t dscap, memcap;
	void *ds_addr;
	struct dax_device *dax_dev;
	struct request_queue *queue;
	int read_write;
	char name[40];
};

static struct l4bdds_device device[NR_DEVS];

static void transfer(struct l4bdds_device *dev,
		     char *ds_addr, unsigned long size,
		     char *buffer, int write)
{
	if ((ds_addr - (char *)dev->ds_addr + size) > dev->ds_size) {
		pr_notice("l4bdds: access beyond end of device (%p %ld)\n",
		          ds_addr, size);
		return;
	}

	if (write)
		memcpy(ds_addr, buffer, size);
	else
		memcpy(buffer, ds_addr, size);
}

static void request(struct request_queue *q)
{
	struct request *req;

	while ((req = blk_peek_request(q)) != NULL) {
		struct req_iterator iter;
		struct bio_vec bvec;
		struct l4bdds_device *dev = req->rq_disk->private_data;
		char *ds_addr = dev->ds_addr;

		blk_start_request(req);

		if (blk_rq_is_passthrough(req)) {
			pr_notice("Skip non-CMD request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}

		ds_addr += blk_rq_pos(req) << KERNEL_SECTOR_SHIFT;

		rq_for_each_segment(bvec, req, iter) {
			transfer(dev, ds_addr,
			         bvec.bv_len,
			         page_address(bvec.bv_page) + bvec.bv_offset,
			         rq_data_dir(req) == WRITE);
			ds_addr += bvec.bv_len;
		}
		__blk_end_request_all(req, 0);
	}
}



static int getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct l4bdds_device *d = bdev->bd_disk->private_data;
	geo->cylinders = d->ds_size >> 11;
	geo->heads     = 64;
	geo->sectors   = 32;
	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations ops = {
	.owner         = THIS_MODULE,
	.getgeo        = getgeo,
};

static long l4bdds_dax_direct_access(struct dax_device *dax_dev,
                                     pgoff_t pgoff, long nr_pages,
                                     void **kaddr, pfn_t *pfn)
{
	struct l4bdds_device *dev = dax_get_private(dax_dev);
	loff_t off = pgoff;

	if (unlikely(off + (nr_pages * PAGE_SIZE) > dev->ds_size))
		return -EIO;

	*kaddr = dev->ds_addr + off;
	*pfn = phys_to_pfn_t(__pa(*kaddr), PFN_DEV);

	return (dev->ds_size - off) / PAGE_SIZE;
}

static const struct dax_operations l4bdds_dax_ops = {
	.direct_access = l4bdds_dax_direct_access,
};

static int __init l4bdds_init_one(int nr)
{
	int ret;
	l4re_ds_stats_t stat;

	if (device[nr].read_write) {
		if ((ret = l4x_query_and_get_cow_ds(device[nr].name, "l4bdds",
		                                    &device[nr].memcap,
		                                    &device[nr].dscap,
		                                    &device[nr].ds_addr,
		                                    &stat, 1))) {
			pr_err("l4bdds: Failed to get file %s (rw): %s(%d)\n",
			       device[nr].name, l4sys_errtostr(ret), ret);
			return ret;
		}
	} else {
		if ((ret = l4x_query_and_get_ds(device[nr].name, "l4bdds",
		                                &device[nr].dscap,
		                                &device[nr].ds_addr, &stat))) {
			pr_err("l4bdds: Failed to get file %s (ro): %s(%d)\n",
			       device[nr].name, l4sys_errtostr(ret), ret);
			return ret;
		}
	}

	ret = -ENODEV;
	device[nr].ds_size = l4_round_page(stat.size);

	spin_lock_init(&device[nr].lock);

	/* Get a request queue. */
	device[nr].queue = blk_init_queue(request, &device[nr].lock);
	if (device[nr].queue == NULL)
		goto out1;

	blk_set_default_limits(&device[nr].queue->limits);

	/* gendisk structure. */
	device[nr].gd = alloc_disk(16);
	if (!device[nr].gd)
		goto out2;
	device[nr].gd->major        = major_num;
	device[nr].gd->first_minor  = nr * 16;
	device[nr].gd->fops         = &ops;
	device[nr].gd->private_data = &device[nr];
	snprintf(device[nr].gd->disk_name, sizeof(device[nr].gd->disk_name),
	         "l4bdds%d", nr);
	set_capacity(device[nr].gd, device[nr].ds_size >> KERNEL_SECTOR_SHIFT);
	if (!(stat.flags & L4RE_DS_MAP_FLAG_RW))
		set_disk_ro(device[nr].gd, 1);

	device[nr].gd->queue = device[nr].queue;
	add_disk(device[nr].gd);

	blk_queue_flag_set(QUEUE_FLAG_DAX, device[nr].queue);
	device[nr].dax_dev = alloc_dax(&device[nr], device[nr].gd->disk_name,
	                               &l4bdds_dax_ops);
	if (!device[nr].dax_dev)
		goto out3;

	pr_info("l4bdds: Disk '%s' size = %lu KB (%lu MB) flags=%lx addr=%p major=%d\n",
	        device[nr].name, device[nr].ds_size >> 10,
	        device[nr].ds_size >> 20,
	        stat.flags, device[nr].ds_addr, device[nr].gd->major);

	return 0;

out3:
	kill_dax(device[nr].dax_dev);
	put_dax(device[nr].dax_dev);
out2:
	blk_cleanup_queue(device[nr].queue);
out1:
	if (device[nr].read_write)
		l4x_detach_and_free_cow_ds(device[nr].memcap,
		                           device[nr].dscap,
		                           device[nr].ds_addr);
	else
		l4x_detach_and_free_ds(device[nr].dscap,
		                       device[nr].ds_addr);

	return ret;
}

static int __init l4bdds_init(void)
{
	int i;

	if (!devs_pos) {
		pr_info("l4bdds: No name given, not starting.\n");
		return 0;
	}

	/* Register device */
	major_num = register_blkdev(major_num, "l4bdds");
	if (major_num < 0) {
		pr_warn("l4bdds: unable to get major number\n");
		return -ENODEV;
	}

	for (i = 0; i < devs_pos; ++i)
		l4bdds_init_one(i);

	return 0;
}

static void __exit l4bdds_exit_one(int nr)
{
	kill_dax(device[nr].dax_dev);
	put_dax(device[nr].dax_dev);
	del_gendisk(device[nr].gd);
	put_disk(device[nr].gd);
	blk_cleanup_queue(device[nr].queue);

	if (device[nr].read_write)
		l4x_detach_and_free_cow_ds(device[nr].memcap,
		                           device[nr].dscap,
		                           device[nr].ds_addr);
	else
		l4x_detach_and_free_ds(device[nr].dscap,
		                       device[nr].ds_addr);
}

static void __exit l4bdds_exit(void)
{
	int i;

	if (!devs_pos)
		return;

	for (i = 0; i < devs_pos; i++)
		l4bdds_exit_one(i);

	unregister_blkdev(major_num, "l4bdds");
}

module_init(l4bdds_init);
module_exit(l4bdds_exit);

static int l4bdds_setup(const char *val, const struct kernel_param *kp)
{
	const char *p = NULL;
	unsigned s, l = strlen(val);

	if (devs_pos >= NR_DEVS) {
		pr_err("l4bdds: Too many block devices specified\n");
		return 1;
	}
	if (l > 3 && !strcmp(val + l - 3, ",rw")) {
		device[devs_pos].read_write = 1;
		p = val + l - 3;
	}
	s = p ? p - val : (sizeof(device[devs_pos].name) - 1);
	strlcpy(device[devs_pos].name, val, s + 1);
	devs_pos++;
	return 0;
}

module_param_call(add, l4bdds_setup, NULL, NULL, 0200);
MODULE_PARM_DESC(add, "Use one l4bdds.add=name[,rw] for each block device to add, name queried in namespace");
