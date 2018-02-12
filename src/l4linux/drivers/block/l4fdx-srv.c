
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/statfs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/syscalls.h>
#include "../../kernel/uid16.h"

#include <linux/root_dev.h>

#include <asm/server/fdx-srv.h>

#include <asm/generic/l4lib.h>
#include <asm/generic/setup.h>

#include <l4/re/env.h>
#include <l4/libfdx/fdx-defs.h>

#include <l4/sys/kdebug.h>

#include <l4/sys/ktrace.h>
#include <linux/file.h>

static struct workqueue_struct *khelper_wq;

enum {
	DEFAULT_UID             = 65535,
	DEFAULT_GID             = 65535,
	DEFAULT_OPEN_FLAGS_MASK = O_RDWR, /* Mask of allowed open flags */
};

struct l4fdx_client {
	unsigned enabled;
	unsigned uid;
	unsigned gid;
	unsigned openflags_mask;
	unsigned flag_nogrow;
	char *basepath;
	char *filterpath;
	char *event_program;
	unsigned basepath_len;
	unsigned filterpath_len;
	char *capname;
	unsigned long long max_file_size;

	struct work_struct create_work;
	struct work_struct add_device_work;
	l4_cap_idx_t cap;
	struct file *files[5]; /* Dynamic? But needs limit */

	wait_queue_head_t event;
	l4fdx_srv_obj srv_obj;

	struct list_head req_list;
	spinlock_t req_list_lock;
};

static char *global_event_program;

L4_EXTERNAL_FUNC(l4x_fdx_srv_create);
L4_EXTERNAL_FUNC(l4x_fdx_srv_create_name);
L4_EXTERNAL_FUNC(l4x_fdx_srv_add_event);
L4_EXTERNAL_FUNC(l4x_fdx_srv_trigger);
L4_EXTERNAL_FUNC(l4x_fdx_srv_get_srv_data);
L4_EXTERNAL_FUNC(l4x_fdx_srv_objsize);

L4_EXTERNAL_FUNC(l4x_fdx_factory_create);
L4_EXTERNAL_FUNC(l4x_fdx_factory_objsize);

enum internal_request_type {
	L4FS_REQ_OPEN, L4FS_REQ_READ, L4FS_REQ_WRITE, L4FS_REQ_CLOSE,
	L4FS_REQ_FSTAT64,
};

struct internal_request {
	struct list_head head;
	enum internal_request_type type;
	unsigned client_req_id;
	union {
		struct {
			char path[56];
			int flags;
			unsigned mode;
		} open;
		struct {
			unsigned fid;
			loff_t   offset;
			size_t   size;
			unsigned shm_offset;
		} read_write;
		struct {
			unsigned fid;
			unsigned shm_offset;
		} fstat;
		struct {
			unsigned fid;
		} close;
	};
};

static int validate_path(const char *path, unsigned len)
{
	unsigned i;

	if (len < 2)
		return 1;

	/* This is very simple and catches too much but it actually should
	 * not be relevant for our use-case */
	for (i = 1; i < len; ++i)
		if (path[i - 1] == '.' && path[i] == '.')
			return 0;

	return 1;
}

static int set_free_fdxslot(struct l4fdx_client *c, struct file *f)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(c->files); ++i)
		if (!c->files[i]) {
			c->files[i] = f;
			return i;
		}

	return -1;

}

static void
add_request(l4fdx_srv_obj srv_obj, struct internal_request *r)
{
	struct l4fdx_client *d = srv_obj->client;
	unsigned long flags;
	spin_lock_irqsave(&d->req_list_lock, flags);
	list_add(&r->head, &d->req_list);
	spin_unlock_irqrestore(&d->req_list_lock, flags);
	wake_up(&srv_obj->client->event);
}

static
struct internal_request *
pop_request(l4fdx_srv_obj srv_obj)
{
	struct l4fdx_client *d = srv_obj->client;
	struct internal_request *r;
	spin_lock_irq(&d->req_list_lock);
	r = list_first_entry(&d->req_list, struct internal_request, head);
	list_del(&r->head);
	spin_unlock_irq(&d->req_list_lock);

	return r;
}

static void res_event(l4fdx_srv_obj srv_obj,
                      struct l4fdx_result_t *r, unsigned client_req_id)
{
	L4XV_V(fl);

	r->time = l4_kip_clock(l4lx_kinfo);
	r->payload.client_req_id = client_req_id;

	L4XV_L(fl);
	l4x_fdx_srv_add_event(srv_obj, r);
	l4x_fdx_srv_trigger(srv_obj);
	L4XV_U(fl);
}

static void call_fdx_event(struct l4fdx_client *c,
                           char *command, char const *path, int umh_wait)
{
	int ret;
	char *argv[4], *envp[4];
	char *prg = NULL;
	char client[40];

	BUG_ON(umh_wait & UMH_NO_WAIT); /* Use of memory for arguments */

	if (c->event_program)
		prg = c->event_program;
	else if (global_event_program)
		prg = global_event_program;
	else
		return;

	snprintf(client, sizeof(client), "FDX_CLIENT=%s",
	         c->capname ? c->capname : "");

	argv[0] = prg;
	argv[1] = command;
	argv[2] = (char *)path;
	argv[3] = NULL;

	envp[0] = "HOME=/";
	envp[1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
	envp[2] = client;
	envp[3] = NULL;

	ret = call_usermodehelper(argv[0], argv, envp, umh_wait);
	if (ret < 0)
		pr_err("l4fdx: Failed to call user event (%d)\n", ret);
}

static void fn_open(l4fdx_srv_obj srv_obj, struct internal_request *r)
{
	struct file *f;
	struct l4fdx_result_t ret;
	struct l4fdx_client *c = srv_obj->client;
	int err, fid = -1;
	char const *path = r->open.path;

	if (c->basepath) {
		unsigned l = strlen(path);
		char *s = kmalloc(c->basepath_len + l + 1, GFP_KERNEL);
		if (!s) {
			err = -ENOMEM;
			goto out;
		}
		strncpy(s, c->basepath, c->basepath_len);
		strncpy(s + c->basepath_len, path, l);
		s[c->basepath_len + l] = 0;

		if (!validate_path(s, c->basepath_len + l)) {
			kfree(s);
			err = -EINVAL;
			goto out;
		}

		path = s;
	}

	call_fdx_event(c, "pre-open", path, UMH_WAIT_PROC);

	f = filp_open(path, r->open.flags & c->openflags_mask, r->open.mode);
	if (IS_ERR(f)) {
		err = PTR_ERR(f);
	} else {
		fid = set_free_fdxslot(c, f);
		if (fid == -1) {
			filp_close(f, NULL);
			err = -ENOMEM;
		} else
			err = 0;
	}

	if (c->flag_nogrow && err == 0) {
		struct kstat stat;
		int r = vfs_getattr(&f->f_path, &stat,
		                    STATX_SIZE, AT_STATX_SYNC_AS_STAT);
		c->max_file_size = r ? 0 : stat.size;
	}

	call_fdx_event(c, "post-open", path, UMH_WAIT_EXEC);

	if (c->basepath)
		kfree(path);

out:
	ret.payload.fid = fid;
	ret.payload.ret = err;
	res_event(srv_obj, &ret, r->client_req_id);

	kfree(r);
}

static void fn_fstat64(l4fdx_srv_obj srv_obj, struct internal_request *r)
{
	struct kstat stat;
	struct l4fdx_result_t ret;
	struct l4fdx_client *c = srv_obj->client;
	struct l4x_fdx_srv_data *data = l4x_fdx_srv_get_srv_data(srv_obj);
	int err, fid = r->fstat.fid;
	unsigned long shm_addr = data->shm_base + r->fstat.shm_offset;

	if (   sizeof(struct l4fdx_stat_t *) > data->shm_size
	    || fid >= ARRAY_SIZE(c->files)
	    || r->fstat.shm_offset + sizeof(struct l4fdx_stat_t *) > data->shm_size) {
		ret.payload.ret = -EINVAL;
		goto out;
	}

	err = vfs_getattr(&c->files[fid]->f_path, &stat,
	                  STATX_ALL, AT_STATX_SYNC_AS_STAT);

	if (!err) {
		struct l4fdx_stat_t *b;

		b = (struct l4fdx_stat_t *)shm_addr;

		// is this ok to munge this like this?
		if (S_ISBLK(stat.mode)) {
			struct block_device *bdev;
			bdev = blkdev_get_by_dev(stat.rdev, FMODE_READ, NULL);
			if (IS_ERR(bdev))
				goto out_err;

			b->blksize = 512;
			b->blocks  = bdev->bd_part->nr_sects;
			b->size    = b->blocks * 512;

			if (0)
				pr_info("l4fdx: fs: %lld %lld %lld  %d:%d\n",
				        b->size, b->blocks, b->blksize,
				        MAJOR(stat.rdev), MINOR(stat.rdev));

			blkdev_put(bdev, FMODE_READ);
		} else {
			b->size    = stat.size;
			b->blocks  = stat.blocks;
			b->blksize = stat.blksize;
		}

		b->dev        = stat.dev;
		b->ino        = stat.ino;
		b->mode       = stat.mode;
		b->nlink      = stat.nlink;
		b->rdev       = stat.rdev;
		b->uid        = from_kuid(&init_user_ns, stat.uid);
		b->gid        = from_kgid(&init_user_ns, stat.gid);
		b->atime.sec  = stat.atime.tv_sec;
		b->atime.nsec = stat.atime.tv_nsec;
		b->mtime.sec  = stat.mtime.tv_sec;
		b->mtime.nsec = stat.mtime.tv_nsec;
		b->ctime.sec  = stat.ctime.tv_sec;
		b->ctime.nsec = stat.ctime.tv_nsec;
	}

out_err:
	ret.payload.ret = err;

out:
	ret.payload.fid = fid;
	res_event(srv_obj, &ret, r->client_req_id);

	kfree(r);
}

static void fn_read(l4fdx_srv_obj srv_obj, struct internal_request *r)
{
	struct l4fdx_result_t ret;
	struct l4x_fdx_srv_data *data = l4x_fdx_srv_get_srv_data(srv_obj);
	long res;
	int fid = r->read_write.fid;
	unsigned long shm_addr = data->shm_base + r->read_write.shm_offset;

	if (   r->read_write.size > data->shm_size
	    || fid >= ARRAY_SIZE(srv_obj->client->files)
	    || r->read_write.shm_offset + r->read_write.size > data->shm_size) {
		ret.payload.ret = -EINVAL;
		goto out;
	}

	res = vfs_read(srv_obj->client->files[fid],
	               (char *)shm_addr,
	               r->read_write.size, &r->read_write.offset);

	if (res < 0)
		pr_err("vfs_read error: %ld\n", res);

	ret.payload.ret        = res;

out:
	ret.payload.fid = fid;
	res_event(srv_obj, &ret, r->client_req_id);

	kfree(r);
}

static void fn_write(l4fdx_srv_obj srv_obj, struct internal_request *r)
{
	struct l4fdx_result_t ret;
	long res;
	int fid = r->read_write.fid;
	struct l4fdx_client *c = srv_obj->client;
	struct l4x_fdx_srv_data *data = l4x_fdx_srv_get_srv_data(srv_obj);

	if (   r->read_write.size > data->shm_size
	    || fid >= ARRAY_SIZE(srv_obj->client->files)
	    || r->read_write.shm_offset + r->read_write.size > data->shm_size) {
		ret.payload.ret = -EINVAL;
		goto out;
	}

	if (c->flag_nogrow) {
		if (r->read_write.offset + r->read_write.size > c->max_file_size) {
			ret.payload.ret = -EINVAL;
			goto out;
		}
	}

	res = vfs_write(srv_obj->client->files[fid],
	                (char *)data->shm_base + r->read_write.shm_offset,
	                r->read_write.size, &r->read_write.offset);

	if (res < 0)
		pr_err("vfs_write error: %ld\n", res);

	ret.payload.ret       = res;

out:
	ret.payload.fid       = fid;
	res_event(srv_obj, &ret, r->client_req_id);
	kfree(r);
}

static void fn_close(l4fdx_srv_obj srv_obj, struct internal_request *r)
{
	struct l4fdx_result_t ret;
	long res;
	int fid = r->read_write.fid;
	struct l4fdx_client *c = srv_obj->client;

	if (fid >= ARRAY_SIZE(c->files)) {
		ret.payload.ret = -EINVAL;
		goto out;
	}

	res = filp_close(c->files[fid], NULL);

	c->files[fid] = NULL;

	ret.payload.ret = res;

out:
	ret.payload.fid = -1;
	res_event(srv_obj, &ret, r->client_req_id);
	kfree(r);
}

static int should_stop = 0;

static int fn(void *d)
{
	struct l4fdx_client *client = d;
	int err;

	err = __sys_setregid(client->gid, client->gid);
	err = __sys_setreuid(client->uid, client->uid);
	err = ksys_setsid();

	if (client->capname)
		snprintf(current->comm, sizeof(current->comm),
		         "l4xfdx-%s", client->capname);
	else
		snprintf(current->comm, sizeof(current->comm),
		         "l4xfdx-%lx",
	                 (client->cap >> L4_CAP_SHIFT) & 0xfffffff);
	current->comm[sizeof(current->comm) - 1] = 0;

	set_user_nice(current, -20);

	while (!should_stop || !list_empty(&client->req_list)) {
		struct internal_request *r;

		wait_event_interruptible(client->event,
		                         !list_empty(&client->req_list)
		                         || should_stop);

		r = pop_request(client->srv_obj);

		switch (r->type) {
			case L4FS_REQ_OPEN:
				fn_open(client->srv_obj, r);
				break;
			case L4FS_REQ_READ:
				fn_read(client->srv_obj, r);
				break;
			case L4FS_REQ_WRITE:
				fn_write(client->srv_obj, r);
				break;
			case L4FS_REQ_CLOSE:
				fn_close(client->srv_obj, r);
				break;
			case L4FS_REQ_FSTAT64:
				fn_fstat64(client->srv_obj, r);
				break;
			default:
				pr_err("l4fdx: Invalid type=%d\n", r->type);
				break;
		}
	}

	return 0;
}


static L4_CV
int b_open(l4fdx_srv_obj srv_obj, unsigned client_req_id,
           const char *path, unsigned len, int flags, unsigned mode)
{
	struct internal_request *r;
	struct l4fdx_client *c = srv_obj->client;

	if (c->filterpath) {
		if (len < c->filterpath_len)
			return -EPERM;

		if (strncmp(c->filterpath, path, c->filterpath_len))
			return -EPERM;
	}

	r = kmalloc(sizeof(struct internal_request), GFP_ATOMIC);
	if (!r)
		return -ENOMEM;

	r->type = L4FS_REQ_OPEN;
	r->client_req_id = client_req_id;
	strlcpy(r->open.path, path, min((size_t)len + 1, sizeof(r->open.path)));
	r->open.flags = flags;
	r->open.mode  = mode;

	add_request(srv_obj, r);

	return 0;
}

/* The kmalloc in here probably does not help for performance so think of
 * something smarter. The GFP_ATOMIC could also be avoided.
 * What about some shared memory between client and server ... */
#define SETUP_REQUEST(client, client_req_id, fid, t) \
	struct internal_request *r; \
	\
	if (fid >= ARRAY_SIZE(client->files) || !client->files[fid]) \
		return -ENOMEM; \
	\
	r = kmalloc(sizeof(struct internal_request), GFP_ATOMIC); \
	if (!r) \
		return -ENOMEM; \
	\
	r->type = t; \
	r->client_req_id = client_req_id;


static L4_CV
int b_fstat(l4fdx_srv_obj srv_obj, unsigned client_req_id, int fid,
            unsigned shm_off)
{
	SETUP_REQUEST(srv_obj->client, client_req_id, fid, L4FS_REQ_FSTAT64);
	r->fstat.fid        = fid;
	r->fstat.shm_offset = shm_off;
	add_request(srv_obj, r);
	return 0;
}

static L4_CV
int b_read(l4fdx_srv_obj srv_obj, unsigned client_req_id,
            int fid, unsigned long long offset, size_t sz, unsigned shm_off)
{
	SETUP_REQUEST(srv_obj->client, client_req_id, fid, L4FS_REQ_READ);
	r->read_write.offset     = offset;
	r->read_write.size       = sz;
	r->read_write.fid        = fid;
	r->read_write.shm_offset = shm_off;

	add_request(srv_obj, r);
	return 0;
}

static L4_CV
int b_write(l4fdx_srv_obj srv_obj, unsigned client_req_id,
            int fid, unsigned long long offset, size_t sz, unsigned shm_off)
{
	SETUP_REQUEST(srv_obj->client, client_req_id, fid, L4FS_REQ_WRITE);
	r->read_write.offset     = offset;
	r->read_write.size       = sz;
	r->read_write.fid        = fid;
	r->read_write.shm_offset = shm_off;

	add_request(srv_obj, r);
	return 0;
}

static L4_CV
int b_close(l4fdx_srv_obj srv_obj, unsigned client_req_id, int fid)
{
	SETUP_REQUEST(srv_obj->client, client_req_id, fid, L4FS_REQ_CLOSE);
	r->close.fid = fid;

	add_request(srv_obj, r);
	return 0;
}

static struct l4x_fdx_srv_ops b_ops = {
	.open    = b_open,
	.read    = b_read,
	.write   = b_write,
	.fstat   = b_fstat,
	.close   = b_close,
};

/* --------------------------------------------------------------- */

static struct class l4fdx_class = {
	.name = "l4fdx",
	.owner = THIS_MODULE,
};

static void create_server(struct work_struct *work)
{
	struct l4fdx_client *client =
	       container_of(work, struct l4fdx_client, create_work);
	int pid;

	pid = kernel_thread(fn, client, CLONE_FS | CLONE_SIGHAND);

	if (pid < 0)
		pr_err("kernel_thread creation failed\n");
}

static inline struct l4fdx_client *to_client(struct device *dev)
{
	return (struct l4fdx_client *)dev_get_drvdata(dev);
}

static ssize_t
client_uid_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%u\n", to_client(dev)->uid);
}

static ssize_t
client_uid_store(struct device *dev, struct device_attribute *attr,
                 const char *buf, size_t size)
{
	if (to_client(dev)->enabled)
		return -EINVAL;
	return kstrtoint(buf, 0, &to_client(dev)->uid) ? : size;
}

static struct device_attribute dev_client_uid_attr
	= __ATTR(uid, 0600, client_uid_show, client_uid_store);

static ssize_t
client_gid_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	return sprintf(buf, "%u\n", to_client(dev)->gid);
}

static ssize_t
client_gid_store(struct device *dev, struct device_attribute *attr,
                 const char *buf, size_t size)
{
	if (to_client(dev)->enabled)
		return -EINVAL;
	return kstrtoint(buf, 0, &to_client(dev)->gid) ? : size;
}

static struct device_attribute dev_client_gid_attr
	= __ATTR(gid, 0600, client_gid_show, client_gid_store);

static ssize_t
client_enable_show(struct device *dev, struct device_attribute *attr,
                   char *buf)
{
	return sprintf(buf, "%u\n", to_client(dev)->enabled);
}

static ssize_t
client_enable_store(struct device *dev, struct device_attribute *attr,
                    const char *buf, size_t size)
{
	struct l4fdx_client *client = to_client(dev);
	void *objmem;

	if (client->enabled)
		return -EINVAL;

	objmem = kmalloc(l4x_fdx_srv_objsize(), GFP_KERNEL);
	if (!objmem)
		return -ENOMEM;

	client->enabled = 1;

	INIT_WORK(&client->create_work, create_server);

	client->srv_obj = L4XV_FN(l4fdx_srv_obj,
	                          l4x_fdx_srv_create_name(l4x_cpu_thread_get_cap(0),
	                                                  client->capname,
	                                                  &b_ops, client,
	                                                  objmem));

	if (IS_ERR(client->srv_obj)) {
		kfree(objmem);
		return PTR_ERR(client->srv_obj);
	}

	queue_work(khelper_wq, &client->create_work);

	return size;
}

static struct device_attribute dev_client_enable_attr
	= __ATTR(enable, 0600, client_enable_show, client_enable_store);

static ssize_t
client_openflags_mask_show(struct device *dev, struct device_attribute *attr,
		           char *buf)
{
	return sprintf(buf, "0%o\n", to_client(dev)->openflags_mask);
}

static ssize_t
client_openflags_mask_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t size)
{
	if (to_client(dev)->enabled)
		return -EINVAL;
	return kstrtoint(buf, 0, &to_client(dev)->openflags_mask) ? : size;
}

static struct device_attribute dev_client_openflags_mask_attr
	= __ATTR(openflags_mask, 0600,
                 client_openflags_mask_show, client_openflags_mask_store);

static ssize_t
client_flag_nogrow_show(struct device *dev, struct device_attribute *attr,
                        char *buf)
{
	return sprintf(buf, "%d\n", to_client(dev)->flag_nogrow);
}

static ssize_t
client_flag_nogrow_store(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t size)
{
	if (to_client(dev)->enabled)
		return -EINVAL;
	return kstrtoint(buf, 0, &to_client(dev)->flag_nogrow) ? : size;
}

static struct device_attribute dev_client_flag_nogrow_attr
	= __ATTR(nogrow, 0600,
                 client_flag_nogrow_show, client_flag_nogrow_store);

static ssize_t
get_string(struct l4fdx_client *client,
           char **s, unsigned *s_len,
           const char *buf, size_t size)
{
	size_t l = 0;

	if (client->enabled)
		return -EINVAL;

	if (*s)
		kfree(*s);

	while (l < size && buf[l] && buf[l] != '\n')
		++l;
	*s     = kstrndup(buf, l, GFP_KERNEL);
	if (s_len)
		*s_len = l;
	return size;
}

static ssize_t
client_basepath_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	if (!to_client(dev)->basepath)
		return 0;

	return sprintf(buf, "%s\n", to_client(dev)->basepath);
}

static ssize_t
client_basepath_store(struct device *dev, struct device_attribute *attr,
                      const char *buf, size_t size)
{
	return get_string(to_client(dev),
	                  &to_client(dev)->basepath,
	                  &to_client(dev)->basepath_len,
	                  buf, size);
}

static struct device_attribute dev_client_basepath_attr
	= __ATTR(basepath, 0600, client_basepath_show, client_basepath_store);

static ssize_t
client_filterpath_show(struct device *dev, struct device_attribute *attr,
                       char *buf)
{
	if (!to_client(dev)->filterpath)
		return 0;

	return sprintf(buf, "%s\n", to_client(dev)->filterpath);
}

static ssize_t
client_filterpath_store(struct device *dev, struct device_attribute *attr,
                        const char *buf, size_t size)
{
	return get_string(to_client(dev),
	                  &to_client(dev)->filterpath,
	                  &to_client(dev)->filterpath_len,
	                  buf, size);
}

static struct device_attribute dev_client_filterpath_attr
	= __ATTR(filterpath, 0600, client_filterpath_show, client_filterpath_store);


static ssize_t
client_event_program_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
	if (!to_client(dev)->event_program)
		return 0;

	return sprintf(buf, "%s\n", to_client(dev)->event_program);
}

static ssize_t
client_event_program_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t size)
{
	return get_string(to_client(dev),
	                  &to_client(dev)->event_program,
	                  NULL, buf, size);
}

static struct device_attribute dev_client_event_program_attr
	= __ATTR(event_program, 0600,
	         client_event_program_show, client_event_program_store);

static struct attribute *client_dev_attrs[] = {
	&dev_client_enable_attr.attr,
	&dev_client_uid_attr.attr,
	&dev_client_gid_attr.attr,
	&dev_client_openflags_mask_attr.attr,
	&dev_client_flag_nogrow_attr.attr,
	&dev_client_basepath_attr.attr,
	&dev_client_filterpath_attr.attr,
	&dev_client_event_program_attr.attr,
	NULL
};

static struct attribute_group client_dev_attr_group = {
	.attrs = client_dev_attrs,
};

static void add_device(struct work_struct *work)
{
	struct device *client_dev;
	struct l4fdx_client *client =
	       container_of(work, struct l4fdx_client, add_device_work);

	if (client->capname)
		client_dev = device_create(&l4fdx_class, NULL, MKDEV(0, 0),
		                           client,
		                           "client-%s", client->capname);
	else
		client_dev = device_create(&l4fdx_class, NULL, MKDEV(0, 0),
		                           client,
		                           "client-%lx",
		                           client->cap >> L4_CAP_SHIFT);
	if (IS_ERR(client_dev)) {
		pr_warn("l4xfdx: device_create failed.\n");
	} else {
		int e;
		e = sysfs_create_group(&client_dev->kobj,
		                       &client_dev_attr_group);
		if (e)
			pr_warn("l4xfdx: sysfs_create_group failed (%d)\n", e);
	}
}

L4_CV static
int l4xfdx_factory_create(struct l4x_fdx_srv_factory_create_data *data,
                          l4_cap_idx_t *client_cap)
{
	struct l4fdx_client *client;
	void *objmem;
	int ret;

	BUG_ON(!irqs_disabled()); /* utcb-data still hot */

	client = kzalloc(sizeof(*client), GFP_ATOMIC);
	if (!client)
		return -ENOMEM;

	ret = -ENOMEM;
	objmem = kmalloc(l4x_fdx_srv_objsize(), GFP_ATOMIC);
	if (!objmem)
		goto out_free_client;

	client->enabled = 1;
	if (data->opt_flags & L4X_FDX_SRV_FACTORY_HAS_UID)
		client->uid = data->uid;
	else
		client->uid = DEFAULT_UID;

	if (data->opt_flags & L4X_FDX_SRV_FACTORY_HAS_GID)
		client->gid = data->gid;
	else
		client->gid = DEFAULT_GID;

	if (data->opt_flags & L4X_FDX_SRV_FACTORY_HAS_OPENFLAGS_MASK)
		client->openflags_mask = data->openflags_mask;
	else
		client->openflags_mask = DEFAULT_OPEN_FLAGS_MASK;

	if (data->opt_flags & L4X_FDX_SRV_FACTORY_HAS_BASEPATH) {
		client->basepath_len = data->basepath_len;
		if (!data->basepath[client->basepath_len - 1])
			--client->basepath_len;
		client->basepath = kstrndup(data->basepath,
		                            client->basepath_len, GFP_ATOMIC);
		if (!client->basepath)
			goto out_free_objmem_and_client_strings;
	}

	if (data->opt_flags & L4X_FDX_SRV_FACTORY_HAS_FILTERPATH) {
		client->filterpath_len = data->filterpath_len;
		if (!data->filterpath[client->filterpath_len - 1])
			--client->filterpath_len;
		client->filterpath = kstrndup(data->filterpath,
		                              client->filterpath_len, GFP_ATOMIC);
		if (!client->filterpath)
			goto out_free_objmem_and_client_strings;
	}

	if (data->opt_flags & L4X_FDX_SRV_FACTORY_HAS_CLIENTNAME) {
		client->capname = kstrndup(data->clientname,
		                           data->clientname_len + 1, GFP_ATOMIC);
		if (!client->capname)
			goto out_free_objmem_and_client_strings;

		client->capname[data->clientname_len] = 0;
	}

	if (data->opt_flags & L4X_FDX_SRV_FACTORY_HAS_FLAG_NOGROW)
		client->flag_nogrow = 1;

	spin_lock_init(&client->req_list_lock);
	INIT_LIST_HEAD(&client->req_list);
	init_waitqueue_head(&client->event);

	INIT_WORK(&client->create_work, create_server);

	client->srv_obj = L4XV_FN(l4fdx_srv_obj,
	                          l4x_fdx_srv_create(l4x_cpu_thread_get_cap(0),
	                                             &b_ops, client, objmem,
	                                             client_cap));

	queue_work(khelper_wq, &client->create_work);

	client->cap = *client_cap;
	INIT_WORK(&client->add_device_work, add_device);
	queue_work(khelper_wq, &client->add_device_work);

	return 0;

out_free_objmem_and_client_strings:
	kfree(objmem);
	kfree(client->basepath);
	kfree(client->filterpath);
	kfree(client->capname);

out_free_client:
	kfree(client);

	return ret;
}

struct l4x_fdx_srv_factory_ops factory_ops;


static ssize_t create_client_store(struct class *cls,
                                   struct class_attribute *attr,
                                   const char *buf, size_t size)
{
	char *capname;
	unsigned l = 0;
	struct l4fdx_client *client;
	struct device *client_dev;
	int ret;
	l4_cap_idx_t c;

	while (l < size && buf[l] && buf[l] != '\n')
		++l;

	capname = kstrndup(buf, l, GFP_KERNEL);

	if (!strcmp(capname, "create_client")) {
		ret = -EINVAL;
		goto out_free_capname;
	}

	c = l4re_env_get_cap(capname);
	if (l4_is_invalid_cap(c)) {
		pr_err("l4xfdx: Cap '%s' not found in L4Re environment\n",
		       capname);
		ret = -ENOENT;
		goto out_free_capname;
	}

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		ret = -ENOMEM;
		goto out_free_capname;
	}

	client->capname        = capname;
	client->uid            = DEFAULT_UID;
	client->gid            = DEFAULT_GID;
	client->openflags_mask = DEFAULT_OPEN_FLAGS_MASK;
	spin_lock_init(&client->req_list_lock);
	INIT_LIST_HEAD(&client->req_list);
	init_waitqueue_head(&client->event);

	client_dev = device_create(&l4fdx_class, NULL, MKDEV(0, 0),
	                           client, "%s", capname);
	if (IS_ERR(client_dev)) {
		pr_err("l4xfdx: device_create of '%s' failed.\n", capname);
		ret = PTR_ERR(client_dev);
		goto out_free_client;
	}

	ret = sysfs_create_group(&client_dev->kobj, &client_dev_attr_group);
	if (ret) {
		pr_err("l4xfdx: sysfs_create_group failed (%d)\n", ret);
		goto out_del_device;
	}

	return size;

out_del_device:
	put_device(client_dev);
	device_unregister(client_dev);
out_free_client:
	kfree(client);
out_free_capname:
	kfree(capname);

	return ret;
}

static struct class_attribute class_attr_create_client
	= __ATTR_WO(create_client);


static ssize_t event_program_show(struct class *cls,
                                  struct class_attribute *attr,
                                  char *buf)
{
	if (!global_event_program)
		return 0;

	return sprintf(buf, "%s\n", global_event_program);
}


static ssize_t event_program_store(struct class *cls,
                                   struct class_attribute *attr,
                                   const char *buf, size_t size)
{
	kfree(global_event_program);

	global_event_program = kstrndup(buf, size + 1, GFP_KERNEL);
	global_event_program[size] = 0;
	if (size && global_event_program[size - 1] == '\n')
		global_event_program[size - 1] = 0;

	return size;
}

static struct class_attribute class_attr_event_program
	= __ATTR_RW(event_program);

static int __init l4xfdx_srv_init(void)
{
	int ret;
	void *objmem;

	khelper_wq = create_singlethread_workqueue("l4fdx-khelper");
	BUG_ON(!khelper_wq);

	ret = class_register(&l4fdx_class);
	if (ret < 0) {
		pr_err("l4xfdx: class_register failed (%d).\n", ret);
		return ret;
	}

	sysfs_attr_init(&class_attr_create_client.attr);
	ret = class_create_file(&l4fdx_class, &class_attr_create_client);
	if (ret) {
		pr_err("l4xfdx: Failed to create file in class (%d)\n", ret);
		goto out_class;
	}

	sysfs_attr_init(&class_attr_event_program.attr);
	ret = class_create_file(&l4fdx_class, &class_attr_event_program);
	if (ret) {
		pr_err("l4xfdx: Failed to create file in class (%d)\n", ret);
		goto out_file1;
	}

	objmem = kmalloc(l4x_fdx_factory_objsize(), GFP_KERNEL);
	if (!objmem) {
		pr_err("l4xfdx: Memory alloation failure\n");
		ret = -ENOMEM;
		goto out_file2;
	}

	factory_ops.create = l4xfdx_factory_create;
	ret = L4XV_FN_i(l4x_fdx_factory_create(l4x_cpu_thread_get_cap(0),
	                                       &factory_ops, objmem));
	if (ret) {
		pr_err("l4xfdx: Failed to create service (%d)\n", ret);
		goto out_mem;
	}

	return 0;

out_mem:
	kfree(objmem);
out_file2:
	class_remove_file(&l4fdx_class, &class_attr_event_program);
out_file1:
	class_remove_file(&l4fdx_class, &class_attr_create_client);
out_class:
	class_unregister(&l4fdx_class);
	return ret;
}

static void __exit l4xfdx_srv_exit(void)
{
	/* TBD */;
}

module_init(l4xfdx_srv_init);
module_exit(l4xfdx_srv_exit);
