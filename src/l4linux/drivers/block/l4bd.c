/*
 * Block driver for l4fdx.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/genalloc.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <asm/api/macros.h>

#include <asm/generic/l4lib.h>
#include <asm/generic/vcpu.h>

#include <l4/libfdx/fdx-c.h>

#include <l4/sys/ktrace.h>

MODULE_AUTHOR("Adam Lackorzynski <adam@l4re.org");
MODULE_DESCRIPTION("Driver for the L4 FDX interface");
MODULE_LICENSE("GPL");

L4_EXTERNAL_FUNC(l4fdxc_init_irq);
L4_EXTERNAL_FUNC(l4fdxc_free);
L4_EXTERNAL_FUNC(l4fdxc_ping);
L4_EXTERNAL_FUNC(l4fdxc_obj_size);
L4_EXTERNAL_FUNC(l4fdxc_request_open);
L4_EXTERNAL_FUNC(l4fdxc_request_fstat);
L4_EXTERNAL_FUNC(l4fdxc_request_write);
L4_EXTERNAL_FUNC(l4fdxc_request_read);
L4_EXTERNAL_FUNC(l4fdxc_request_close);
L4_EXTERNAL_FUNC(l4fdxc_shm_buffer);
L4_EXTERNAL_FUNC(l4fdxc_next_event);
L4_EXTERNAL_FUNC(l4fdxc_free_event);
L4_EXTERNAL_FUNC(l4fdxc_irq);
L4_EXTERNAL_FUNC(l4fdxc_shm_buffer_size);

enum {
	NR_DEVS = 4,
	POLL_INTERVAL = 500,
	MINORS_PER_DISK = 16,
};

static char fdxcap[16];
module_param_string(fdxcap, fdxcap, sizeof(fdxcap), 0);
MODULE_PARM_DESC(fdxcap, "L4 block driver fdx cap (mandatory to enable driver)");

static int major_num = 0;        /* kernel chooses */
module_param(major_num, int, 0);

#define KERNEL_SECTOR_SIZE 512

/*
 * Our request queue.
 */
static struct request_queue *queue;

struct fdx_conn {
	l4fdxc fdxc;
	void *fdxcobj;
	int irq;
	struct gen_pool *pool;
	unsigned long buf;
};

/* Currently we can only talk to one server */
static struct fdx_conn srv_conn;

/*
 * The internal representation of our device.
 */
struct l4bd_device {
	unsigned long size; /* Size in Kbytes */
	spinlock_t lock;
	struct gendisk *gd;
	struct fdx_conn *srv;
	int fdxc_fid;
	int open_flags;
	bool sync_probe;
	unsigned long data[1];
	bool request_handled;
	wait_queue_head_t op_complete;
	char path[60];
};

static struct l4bd_device device[NR_DEVS];
static int nr_devs;

static DEFINE_MUTEX(requests_infligt_lock);

struct inflight {
	struct l4bd_device *d;
	union {
		struct {
			struct request *req;
			void *shm_addr;
			size_t buf_size;
		} req;
		struct {
			unsigned long type;
		} misc;
	};
};

enum {
	REQ_ID_FREE = 0,
	REQ_ID_OPEN,
	REQ_ID_STAT,
	REQ_ID_CLOSE,
	REQ_ID_MAX
};

static inline bool is_ifl_req(struct inflight *ifl)
{
	return ifl->misc.type >= REQ_ID_MAX;
};

static struct inflight *requests_infligt;
static int requests_infligt_num;
static int requests_infligt_last;

static int get_inflight_op(unsigned long tval, struct l4bd_device *d)
{
	unsigned i, r = -ENOMEM;

	mutex_lock(&requests_infligt_lock);

	do {
		i = requests_infligt_last + 1;
		while (1) {
			if (i == requests_infligt_num)
				i = 0;
			if (i == requests_infligt_last)
				break;
			if (requests_infligt[i].misc.type == REQ_ID_FREE) {
				requests_infligt[i].misc.type = tval;
				requests_infligt[i].d = d;
				r = requests_infligt_last = i;
				break;
			}
			++i;
		}

		if (likely(r != -ENOMEM))
			break;

		requests_infligt_last = requests_infligt_num - 1;
		requests_infligt_num *= 2;
		if (requests_infligt_num >= 1024)
			break;
		requests_infligt = krealloc(requests_infligt,
		                            requests_infligt_num
		                             * sizeof(*requests_infligt),
		                            GFP_KERNEL | __GFP_ZERO);
		if (!requests_infligt)
			break;
	} while (1);

	mutex_unlock(&requests_infligt_lock);

	return r;
}

static int get_inflight_req(struct request *req, struct l4bd_device *d)
{
	return get_inflight_op((unsigned long)req, d);
}

static void free_inflight_request(int idx)
{
	requests_infligt[idx].misc.type = REQ_ID_FREE;
}

static void process_read_write_req(struct l4bd_device *d,
                                   struct request *req,
                                   struct inflight *ifl)
{
	if (rq_data_dir(req) != WRITE) {
		struct bio_vec bvec;
		struct req_iterator iter;
		size_t offset = 0;

		rq_for_each_segment(bvec, req, iter) {
			memcpy(page_address(bvec.bv_page) + bvec.bv_offset,
			       (void *)((char *)ifl->req.shm_addr + offset),
			       bvec.bv_len);
			offset += bvec.bv_len;
		}

		BUG_ON(ifl->req.buf_size != offset);
	}

	gen_pool_free(srv_conn.pool,
	              (unsigned long)ifl->req.shm_addr, ifl->req.buf_size);
}

static void handle_request(struct l4fdx_result_t *e)
{
	struct l4fdx_stat_t *stbuf;
	struct l4bd_device *d;
	struct inflight *ifl;
	unsigned req_id = e->payload.client_req_id;
	struct request_queue *q = NULL;

	if (unlikely(req_id >= requests_infligt_num)) {
		pr_err_ratelimited("l4bd: Request return failure\n");
		return;
	}

	ifl = &requests_infligt[req_id];
	d = requests_infligt[req_id].d;

	if (likely(is_ifl_req(ifl))) {
		struct request *req = ifl->req.req;
		unsigned long flags;
		int r = 0;

		if (e->payload.ret >= 0)
			process_read_write_req(d, req, ifl);
		else
			r = e->payload.ret;

		q = d->gd->queue;

		spin_lock_irqsave(q->queue_lock, flags);
		__blk_end_request_all(req, r);
		spin_unlock_irqrestore(q->queue_lock, flags);


	} else {
		switch (ifl->misc.type) {
		case REQ_ID_OPEN:
			d->request_handled = true;
			if (e->payload.ret < 0)
				d->fdxc_fid = e->payload.ret;
			else
				d->fdxc_fid = e->payload.fid;
			wake_up_interruptible(&d->op_complete);
			break;
		case REQ_ID_STAT:
			d->request_handled = true;
			stbuf = (struct l4fdx_stat_t *)d->data[0];
			/* l4bd_device.size is unsigned and in KBytes */
			d->size = (stbuf->size + 1023) >> 10;
			d->data[0] = e->payload.ret;
			wake_up_interruptible(&d->op_complete);
			break;

		case REQ_ID_CLOSE:
			d->request_handled = true;
			d->data[0] = e->payload.ret;
			wake_up_interruptible(&d->op_complete);
			break;
		default:
			pr_err_ratelimited("l4bd: Request return failure\n");
			break;
		};
	}

	free_inflight_request(req_id);

	if (q && blk_queue_stopped(q)) {
		spin_lock(q->queue_lock);
		blk_start_queue(q);
		spin_unlock(q->queue_lock);
	}
}

static irqreturn_t isr(int irq, void *_fdxc)
{
	l4fdxc fdxc = (l4fdxc)_fdxc;
	struct l4fdx_result_t *e;

	while ((e = l4fdxc_next_event(fdxc))) {
		handle_request(e);
		l4fdxc_free_event(fdxc, e);
	}

	return IRQ_HANDLED;
}

static int do_write(struct l4bd_device *d, struct request *req,
                    unsigned req_id,
                    unsigned long long start,
                    unsigned long size)
{
	unsigned long a = gen_pool_alloc(srv_conn.pool, size);
	unsigned long offset = 0;
	struct bio_vec bvec;
	struct req_iterator iter;

	if (a == 0)
		return -ENOMEM;

	requests_infligt[req_id].req.shm_addr = (void *)a;

	rq_for_each_segment(bvec, req, iter) {
		memcpy((void *)(a + offset),
		       page_address(bvec.bv_page) + bvec.bv_offset,
		       bvec.bv_len);
		offset += bvec.bv_len;
	}

	return l4fdxc_request_write(d->srv->fdxc, req_id, d->fdxc_fid,
	                            size, start, a - srv_conn.buf);
}

static int do_read(struct l4bd_device *d, unsigned req_id,
                   unsigned long long start,
                   unsigned long size)
{
	unsigned long a = gen_pool_alloc(srv_conn.pool, size);

	if (a == 0)
		return -ENOMEM;

	requests_infligt[req_id].req.shm_addr = (void *)a;
	return l4fdxc_request_read(d->srv->fdxc, req_id, d->fdxc_fid,
	                           size, start, a - srv_conn.buf);
}

static void do_request(struct request_queue *q)
{
	struct request *req;

	while ((req = blk_fetch_request(q))) {
		unsigned long long s;
		unsigned long size = 0;
		int r = 0;
		struct l4bd_device *d = req->rq_disk->private_data;
		int req_id;

		if (blk_rq_is_passthrough(req)) {
			pr_notice("Skip non-CMD request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}

		s = blk_rq_pos(req) * KERNEL_SECTOR_SIZE;

		req_id = get_inflight_req(req, d);
		if (req_id < 0) {
			pr_info_ratelimited("l4bd: Ran out of inflight "
			                    "storage\n");
			goto do_later;
		}

		size = blk_rq_bytes(req);

		requests_infligt[req_id].req.buf_size = size;

		if (unlikely(((s + size) >> 10) > d->size)) {
			pr_notice("l4bd: access beyond end of device "
			          "(%lld, %ld)\n", s + size, size);
			free_inflight_request(req_id);
			continue;
		}

		BUG_ON(!irqs_disabled());

		if (rq_data_dir(req) == WRITE)
			r = do_write(d, req, req_id, s, size);
		else
			r = do_read(d, req_id, s, size);

		if (unlikely(r < 0)) {
			free_inflight_request(req_id);
			goto do_later;
		}

		continue;
do_later:
		blk_requeue_request(q, req);
		blk_stop_queue(q);
		break;
	}
}

static int l4bd_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct l4bd_device *d = bdev->bd_disk->private_data;
	geo->cylinders = d->size << 5;
	geo->heads     = 4;
	geo->sectors   = 32;
	return 0;
}


/*
 * The device operations structure.
 */
static struct block_device_operations l4bd_ops = {
	.owner = THIS_MODULE,
	.getgeo = l4bd_getgeo,
};

enum init_one_return {
	INIT_OK,
	INIT_ERR_NOT_FOUND,
	INIT_ERR_CREATE_FAILED,
};

static int do_open(struct l4bd_device *d)
{
	int r, req_id;

	while ((req_id = get_inflight_op(REQ_ID_OPEN, d)) < 0)
		msleep(1);

	d->request_handled = false;
	r = L4XV_FN_i(l4fdxc_request_open(d->srv->fdxc, req_id, d->path,
	                                  d->open_flags, 0));

	if (r < 0)
		return r;

	r = wait_event_interruptible(d->op_complete, d->request_handled);
	if (r)
		return r;

	return d->fdxc_fid;
}

static int get_size(struct l4bd_device *d)
{
	int r, req_id;

	d->data[0] = gen_pool_alloc(srv_conn.pool,
	                            sizeof(struct l4fdx_stat_t));

	while ((req_id = get_inflight_op(REQ_ID_STAT, d)) < 0)
		msleep(1);

	d->request_handled = false;
	r = L4XV_FN_i(l4fdxc_request_fstat(d->srv->fdxc, req_id, d->fdxc_fid,
	                                   d->data[0] - srv_conn.buf));

	if (r < 0)
		return r;

	r = wait_event_interruptible(d->op_complete, d->request_handled);
	if (r < 0)
		return r;

	return d->data[0];
}

static int do_close(struct l4bd_device *d)
{
	int r, req_id;

	while ((req_id = get_inflight_op(REQ_ID_CLOSE, d)) < 0)
		msleep(1);

	d->request_handled = false;
	r = L4XV_FN_i(l4fdxc_request_close(d->srv->fdxc, req_id, d->fdxc_fid));

	if (r < 0)
		return r;

	r = wait_event_interruptible(d->op_complete, d->request_handled);
	if (r < 0)
		return r;

	return d->data[0];
}

static enum init_one_return init_one(int idx)
{
	int ret;
	struct l4bd_device *d = &device[idx];

	d->srv = &srv_conn;

	init_waitqueue_head(&d->op_complete);

	d->fdxc_fid = -1;
	ret = do_open(d);
	if (ret == -ENOENT)
		return INIT_ERR_NOT_FOUND;
	else if (ret < 0)
		return INIT_ERR_CREATE_FAILED;

	if (d->fdxc_fid < 0) {
		pr_err("l4bd: did not find %s (%d)\n", d->path, d->fdxc_fid);
		return INIT_ERR_CREATE_FAILED;
	}

	d->size = 0;
	ret = get_size(d);
	if (ret) {
		pr_err("l4bd: Could not stat: %d\n", ret);
		goto out1;
	}

	spin_lock_init(&d->lock);

	/* Get a request queue. One would be enough? */
	queue = blk_init_queue(do_request, &d->lock);
	if (queue == NULL)
		goto out1;

	/* gendisk structure. */
	d->gd = alloc_disk(MINORS_PER_DISK);
	if (!d->gd)
		goto out2;
	d->gd->major        = major_num;
	d->gd->first_minor  = MINORS_PER_DISK * idx;
	d->gd->fops         = &l4bd_ops;
	d->gd->private_data = d;
	snprintf(d->gd->disk_name, sizeof(d->gd->disk_name), "l4bd%d", idx);
	d->gd->disk_name[sizeof(d->gd->disk_name) - 1] = 0;
	set_capacity(d->gd, d->size * 2 /* 2 * kb = 512b-sectors */);
	d->gd->queue = queue;
	add_disk(d->gd);

	pr_info("l4bd: Disk %s (%s,%d,%s,%o) size = %lu KB (%lu MB), dev=%d:%d\n",
	        d->gd->disk_name,
	        fdxcap, d->fdxc_fid, d->path, d->open_flags,
	        d->size, d->size >> 10, major_num, d->gd->first_minor);

	return INIT_OK;

out2:
	blk_cleanup_queue(queue);
out1:
	do_close(d);
	return INIT_ERR_CREATE_FAILED;
}

static int do_probe(void *d)
{
	int i = (long)d;

	while (L4XV_FN_i(l4fdxc_ping(fdxcap)))
		msleep(POLL_INTERVAL);

	while (1) {
		enum init_one_return r = init_one(i);

		if (r == INIT_ERR_CREATE_FAILED)
			return -ENOMEM;
		if (r == INIT_OK)
			return 0;

		msleep(POLL_INTERVAL);
	}
}

static int __init l4bd_init(void)
{
	int i, ret, probe_failed = 0;

	if (!fdxcap[0]) {
		pr_err("l4bd: No fdx cap-name (fdxcap) given, not starting.\n");
		return -ENODEV;
	}

	major_num = register_blkdev(major_num, "l4bd");
	if (major_num <= 0) {
		pr_warn("l4bd: unable to get major number\n");
		return -ENOMEM;
	}

	ret = -ENOMEM;
	srv_conn.fdxcobj = kmalloc(l4fdxc_obj_size(), GFP_KERNEL);
	if (!srv_conn.fdxcobj)
		goto out1;

	ret = L4XV_FN_i(l4fdxc_init_irq(fdxcap, &srv_conn.fdxc, srv_conn.fdxcobj));
	if (ret) {
		pr_err("l4bd: Failed to connect to '%s': %d\n", fdxcap, ret);
		goto out2;
	}

	srv_conn.irq = l4x_register_irq(l4fdxc_irq(srv_conn.fdxc));
	if (srv_conn.irq < 0)
		goto out3;

	ret = request_irq(srv_conn.irq, isr, 0, "l4bd", srv_conn.fdxc);
	if (ret)
		goto out4;

	srv_conn.pool = gen_pool_create(10, -1);
	if (!srv_conn.pool)
		goto out5;

	srv_conn.buf = (unsigned long)l4fdxc_shm_buffer(srv_conn.fdxc);

	ret = gen_pool_add_virt(srv_conn.pool, srv_conn.buf, srv_conn.buf,
	                        l4fdxc_shm_buffer_size(srv_conn.fdxc), -1);
	if (ret)
		goto out6;

	requests_infligt_num = 8;
	requests_infligt = kcalloc(requests_infligt_num,
	                           sizeof(*requests_infligt), GFP_KERNEL);

	if (!requests_infligt)
		goto out6;

	pr_info("l4bd: Found L4fdx server at cap \'%s\'.\n", fdxcap);

	for (i = 0; i < nr_devs; ++i) {
		if (device[i].sync_probe) {
			ret = do_probe((void *)(long)i);
			if (ret < 0)
				probe_failed++;
		} else {
			struct task_struct *k;
			k = kthread_run(do_probe, (void *)(long)i,
			                "l4bd_probe/%d", i);
			if (IS_ERR(k)) {
				ret = PTR_ERR(k);
				probe_failed++;
			}
		}
	}

	if (probe_failed == nr_devs)
		goto out6;

	return 0;

out6:
	gen_pool_destroy(srv_conn.pool);
out5:
	free_irq(srv_conn.irq, srv_conn.fdxc);
out4:
	l4x_unregister_irq(srv_conn.irq);
out3:
	L4XV_FN_v(l4fdxc_free(srv_conn.fdxc));
out2:
	kfree(srv_conn.fdxcobj);
out1:
	unregister_blkdev(major_num, "l4bd");

	return ret;
}

static void __exit l4bd_exit(void)
{
	int i;
	for (i = 0; i < nr_devs; ++i) {
		do_close(&device[i]);
		del_gendisk(device[i].gd);
		put_disk(device[i].gd);
	}
	free_irq(srv_conn.irq, srv_conn.fdxc);
	l4x_unregister_irq(srv_conn.irq);
	L4XV_FN_v(l4fdxc_free(srv_conn.fdxc));
	kfree(srv_conn.fdxcobj);
	gen_pool_destroy(srv_conn.pool);
	unregister_blkdev(major_num, "l4bd");
	blk_cleanup_queue(queue);
}

module_init(l4bd_init);
module_exit(l4bd_exit);

static int l4bd_setup(const char *val, const struct kernel_param *kp)
{
	const char *p = NULL;
	unsigned l = strlen(val);
	ptrdiff_t path_len = strlen(val);

	if (nr_devs >= NR_DEVS) {
		pr_err("l4bd: Too many block devices specified\n");
		return -ENOMEM;
	}
	if (l > 3 && (p = strstr(val, ",rw"))
	    && (*(p + 3) == ',' || *(p + 3) == 0)) {
		path_len = min(path_len, p - val);
		device[nr_devs].open_flags |= O_RDWR;
	}
	if (l > 10 && (p = strstr(val, ",oflags="))) {
		char *e = 0;
		unsigned flags = simple_strtoul(p + 8, &e, 0);

		if (e > p + 8 && (*e == ',' || *e == 0)) {
			device[nr_devs].open_flags |= flags;
			path_len = min(path_len, p - val);
		}
	}
	if (l > 5 && (p = strstr(val, ",sync"))) {
		device[nr_devs].sync_probe = true;
		path_len = min(path_len, p - val);
	}

	if (path_len > sizeof(device[nr_devs].path) - 1)
		return -ENOMEM;

	strlcpy(device[nr_devs].path, val, path_len + 1);
	nr_devs++;
	return 0;
}

module_param_call(add, l4bd_setup, NULL, NULL, 0200);
MODULE_PARM_DESC(add, "Use one l4bd.add=path[,rw][,oflags=0...][,sync] for each block device to add");
