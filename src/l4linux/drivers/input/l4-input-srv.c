#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>

#include <asm/generic/l4lib.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/smp.h>
#include <asm/generic/setup.h>

#include <asm/server/input-srv.h>
#include <l4/re/event_enums.h>

L4_EXTERNAL_FUNC(l4x_srv_input_init);
L4_EXTERNAL_FUNC(l4x_srv_input_trigger);
L4_EXTERNAL_FUNC(l4x_srv_input_add_event);

static int enable;

struct evdev {
	int open;
	struct input_handle handle;
	struct evdev *next;
	struct evdev **prev_next;
};

static struct evdev *first_evdev;

static void l4x_input_fill_info(struct evdev *d, l4re_event_stream_info_t *info)
{
	struct input_dev *dev = d->handle.dev;
	info->stream_id = (l4_umword_t)dev;

	info->id.bustype = dev->id.bustype;
	info->id.vendor  = dev->id.vendor;
	info->id.product = dev->id.product;
	info->id.version = dev->id.version;
#define COPY_BITS(n) memcpy(info->n ##s, dev->n, min(sizeof(info->n ##s), sizeof(dev->n)))

	COPY_BITS(propbit);
	COPY_BITS(evbit);
	COPY_BITS(keybit);
	COPY_BITS(relbit);
	COPY_BITS(absbit);
#undef COPY_BITS
}

static L4_CV int l4x_input_num_streams(void)
{
	int i = 0;
	struct evdev *d = first_evdev;
	for (i = 0; d; i++, d = d->next)
		;
	return i;
}

static L4_CV int l4x_input_stream_info(int idx, l4re_event_stream_info_t *si)
{
	int i = 0;
	struct evdev *d = first_evdev;
	for (i = 0; d && idx < i; i++, d = d->next)
		;

	if (!d)
		return -EINVAL;

	l4x_input_fill_info(d, si);

	return 0;
}

static L4_CV int l4x_input_stream_info_for_id(l4_umword_t id, l4re_event_stream_info_t *si)
{
	struct evdev *d = first_evdev;
	for (; d && (l4_umword_t)(d->handle.dev) != id; d = d->next)
		;

	if (!d)
		return -EINVAL;

	l4x_input_fill_info(d, si);

	return 0;
}

static L4_CV int l4x_input_axis_info(l4_umword_t id, unsigned naxes,
                                     unsigned const *axis, l4re_event_absinfo_t *info)
{
	struct evdev *d = first_evdev;
	struct input_dev *dev;
	unsigned idx;

	for (; d && (l4_umword_t)(d->handle.dev) != id; d = d->next)
		;

	if (!d)
		return -EINVAL;

	dev = d->handle.dev;

	if (!dev->absinfo)
		return -EINVAL;


	for (idx = 0; idx < naxes; idx++) {
		unsigned a = axis[idx];
		if (a >= ABS_CNT)
			return -EINVAL;

		info[idx].value      = input_abs_get_val(dev, a);
		info[idx].min        = input_abs_get_min(dev, a);
		info[idx].max        = input_abs_get_max(dev, a);
		info[idx].fuzz       = input_abs_get_fuzz(dev, a);
		info[idx].flat       = input_abs_get_flat(dev, a);
		info[idx].resolution = input_abs_get_res(dev, a);
	}

	return 0;
}

static struct l4x_input_srv_ops l4x_input_ops = {
	.num_streams        = &l4x_input_num_streams,
	.stream_info        = &l4x_input_stream_info,
	.stream_info_for_id = &l4x_input_stream_info_for_id,
	.axis_info          = &l4x_input_axis_info,
};

/*
 * Pass incoming event to all connected clients.
 */
static void evdev_event(struct input_handle *handle,
			unsigned int type, unsigned int code, int value)
{
	struct l4x_input_event e;

	e.time      = l4_kip_clock(l4lx_kinfo);
	e.type      = type;
	e.code      = code;
	e.value     = value;
	e.stream_id = (unsigned long)handle->dev;

	L4XV_FN_v(l4x_srv_input_add_event(&e));

	if (type == EV_SYN) {
		L4XV_FN_v(l4x_srv_input_trigger());
		return;
	}

	if (0)
		printk("l4-input-srv: EVENT[%p]: %x %x %x\n", &e, type, code, value);
}

/*
 * Create new evdev device. Note that input core serializes calls
 * to connect and disconnect so we don't need to lock evdev_table here.
 */
static int evdev_connect(struct input_handler *handler, struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct evdev *evdev;
	int error;

	printk("l4-input-srv: init: %s\n", dev->name);

	evdev = kzalloc(sizeof(struct evdev), GFP_KERNEL);
	if (!evdev)
		return -ENOMEM;

	evdev->handle.dev = input_get_device(dev);
	evdev->handle.name = "l4-input-srv"; //evdev->name;
	evdev->handle.handler = handler;
	evdev->handle.private = evdev;

	error = input_register_handle(&evdev->handle);
	if (error)
		goto err_free_evdev;

	error = input_open_device(&evdev->handle);
	if (error)
		goto err_unregister;

	evdev->next = first_evdev;
	evdev->prev_next = &first_evdev;
	if (evdev->next)
		evdev->next->prev_next = &evdev->next;

	first_evdev = evdev;
	evdev_event(&evdev->handle, EV_SYN, L4RE_SYN_STREAM_CFG,
	            L4RE_SYN_STREAM_NEW);

	evdev->open = 1;
	return 0;
err_unregister:
	input_unregister_handle(&evdev->handle);
err_free_evdev:
	kfree(evdev);
	return error;
}

static void evdev_disconnect(struct input_handle *handle)
{
	struct evdev *evdev = handle->private;

	evdev_event(handle, EV_SYN, L4RE_SYN_STREAM_CFG, L4RE_SYN_STREAM_CLOSE);

	/* evdev is marked dead so no one else accesses evdev->open */
	if (evdev && evdev->open) {
		input_flush_device(handle, NULL);
		input_close_device(handle);
		evdev->open = 0;
		handle->private = 0;
		if (evdev->next)
			evdev->next->prev_next = evdev->prev_next;

		*evdev->prev_next = evdev->next;
		kfree(evdev);
	}

	input_unregister_handle(handle);
}

static const struct input_device_id evdev_ids[] = {
	{ .driver_info = 1 },	/* Matches all devices */
	{ },			/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, evdev_ids);

static struct input_handler evdev_handler = {
	.event		= evdev_event,
	.connect	= evdev_connect,
	.disconnect	= evdev_disconnect,
	.name		= "l4evdev",
	.id_table	= evdev_ids,
};


static int __init l4evsrv_init(void)
{
	if (!enable)
		return 0;

	printk("L4Re::Event Service\n");

	L4XV_FN_v(l4x_srv_input_init(l4x_cpu_thread_get_cap(0), &l4x_input_ops));

	return input_register_handler(&evdev_handler);
}

static void __exit l4evsrv_exit(void)
{
	input_unregister_handler(&evdev_handler);
}

module_init(l4evsrv_init);
module_exit(l4evsrv_exit);

module_param(enable, int, 0444);
MODULE_PARM_DESC(enable, "Enable input-srv");

MODULE_AUTHOR("Alex W, Adam L");
MODULE_DESCRIPTION("L4 --- Event server");
MODULE_LICENSE("GPL");
