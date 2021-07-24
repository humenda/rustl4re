#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>

#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_inhibitor.h>
#include <l4/re/env.h>
#include <l4/re/event_enums.h>
#include <l4/re/c/inhibitor.h>


#include <asm/generic/l4lib.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/event.h>
#include <asm/generic/vbus.h>

L4_EXTERNAL_FUNC(l4vbus_get_hid);
L4_EXTERNAL_FUNC(l4vbus_get_next_device);
L4_EXTERNAL_FUNC(l4vbus_get_resource);
L4_EXTERNAL_FUNC(l4re_inhibitor_acquire);
L4_EXTERNAL_FUNC(l4re_inhibitor_release);
L4_EXTERNAL_FUNC(l4vbus_get_device);


static l4_cap_idx_t vbus;
static struct l4x_vbus_device *vbus_root;
static struct l4x_event_source vbus_event_source;
static struct work_struct w_suspend;
static struct work_struct w_shutdown;

struct vbus_root_data {
	unsigned inhibitor_state;
};

enum {
	L4X_VBUS_INH_SUSPEND  = 1 << 0,
	L4X_VBUS_INH_SHUTDOWN = 1 << 1,
	L4X_VBUS_INH_WAKEUP   = 1 << 2,
};

static void process_bus_event(struct l4x_event_stream *stream,
                              l4re_event_t const *event)
{
	struct l4x_vbus_device *dev = l4x_vbus_device_from_stream(stream);
	struct l4x_vbus_driver *drv;

	if (!dev->dev.driver)
		return;

	BUG_ON(dev->dev.driver->bus != &l4x_vbus_bus_type);
	drv = l4x_vbus_driver_from_driver(dev->dev.driver);

	if (drv->ops.notify)
		drv->ops.notify(dev, event->type, event->code, event->value);
}

static struct l4x_event_stream *
bus_unknown_event_stream(struct l4x_event_source *src, unsigned long id)
{
	pr_info("l4vbus: no driver for vbus id: %lx registered\n", id);
	return NULL;
}

static struct l4x_event_source_ops vbus_event_ops = {
	.process     = process_bus_event,
	.new_stream  = bus_unknown_event_stream,
};

static int vbus_root_add(struct l4x_vbus_device *device)
{
	int r;
	struct vbus_root_data *d = kzalloc(sizeof(*d), GFP_KERNEL);
	BUG_ON(device->driver_data);
	dev_info(&device->dev, "added vbus root driver");

	if (!d)
		return -ENOMEM;

	device->driver_data = d;
	d->inhibitor_state  = L4X_VBUS_INH_SUSPEND | L4X_VBUS_INH_SHUTDOWN;

	r = L4XV_FN_i(l4re_inhibitor_acquire(vbus, L4VBUS_INHIBITOR_SUSPEND,
	                                     "vbus drivers running"));
	if (r < 0)
		pr_info("L4x: Failed to acquire 'suspend' inhibitor.\n");

	r = L4XV_FN_i(l4re_inhibitor_acquire(vbus, L4VBUS_INHIBITOR_SHUTDOWN,
	                                     "vbus drivers running"));
	if (r < 0)
		pr_info("L4x: Failed to acquire 'shutdown' inhibitor.\n");


	/* l4-vbus events are wakeups per default */
	device_init_wakeup(&device->dev, true);
	return 0;
}

static void vbus_root_shutdown(struct l4x_vbus_device *device)
{
	int r = 0;
	struct vbus_root_data *d = device->driver_data;
	if (!d)
		return;

	if (d->inhibitor_state & L4X_VBUS_INH_SUSPEND)
		r |= L4XV_FN_i(l4re_inhibitor_release(vbus,
		               L4VBUS_INHIBITOR_SUSPEND));
	if (d->inhibitor_state & L4X_VBUS_INH_SHUTDOWN)
		r |= L4XV_FN_i(l4re_inhibitor_release(vbus,
		               L4VBUS_INHIBITOR_SHUTDOWN));
	if (d->inhibitor_state & L4X_VBUS_INH_WAKEUP)
		r |= L4XV_FN_i(l4re_inhibitor_release(vbus,
		               L4VBUS_INHIBITOR_WAKEUP));
	d->inhibitor_state = 0;

	if (r)
		pr_info("L4x: Failed to release inhibitor(s).\n");
}

static int vbus_root_remove(struct l4x_vbus_device *device)
{
	struct vbus_root_data *d = device->driver_data;
	if (!d)
		return 0;

	vbus_root_shutdown(device);
	device->driver_data = NULL;
	kfree(d);
	return 0;
}

static void vbus_root_notify(struct l4x_vbus_device *device, unsigned type,
                             unsigned code, unsigned value)
{
	if (type != L4RE_EV_PM || value != 1) {
		return; /* unhandled */
	}

	switch (code) {
	case L4VBUS_INHIBITOR_SUSPEND:
		schedule_work(&w_suspend);
		break;
	case L4VBUS_INHIBITOR_SHUTDOWN:
		schedule_work(&w_shutdown);
		break;
	case L4VBUS_INHIBITOR_WAKEUP:
		dev_info(&device->dev, "wakeup event\n");
		break;

	default:
		dev_info(&device->dev, "unknown event: %d %d %d\n", type, code, value);
		break;
	}
}

static int vbus_root_suspend(struct device *dev)
{
	l4x_event_source_enable_wakeup(&vbus_event_source,
	                               device_may_wakeup(dev));
	return 0;
}

static int vbus_root_suspend_noirq(struct device *dev)
{
	struct l4x_vbus_device *device = l4x_vbus_device_from_device(dev);
	struct vbus_root_data *d = device->driver_data;
	int r;

	d->inhibitor_state |= L4X_VBUS_INH_WAKEUP;
	r = L4XV_FN_i(l4re_inhibitor_acquire(vbus, L4VBUS_INHIBITOR_WAKEUP,
	                                     "want wakeup from vbus"));
	if (r < 0)
		pr_info("L4x: Failed to acquire 'wakup' inhibitor.\n");
	if (d->inhibitor_state & L4X_VBUS_INH_SUSPEND) {
		d->inhibitor_state &= ~L4X_VBUS_INH_SUSPEND;
		/* releasing the suspend inhibitor may immediately send the
		 * system into suspend, event before l4linux is completely
		 * suspended internally, however this is ok since all hardware
		 * devices should already be suspended correctly.
		 */
		r = L4XV_FN_i(l4re_inhibitor_release(vbus,
		                                     L4VBUS_INHIBITOR_SUSPEND));
		if (r < 0)
			pr_info("L4x: Failed to release 'suspend' inhibitor.\n");
	}
	return 0;
}

static int vbus_root_resume_noirq(struct device *dev)
{
	struct l4x_vbus_device *device = l4x_vbus_device_from_device(dev);
	struct vbus_root_data *d = device->driver_data;
	int r;

	r = L4XV_FN_i(l4re_inhibitor_acquire(vbus, L4VBUS_INHIBITOR_SUSPEND,
	                                     "vbus drivers running"));
	if (r < 0)
		pr_info("L4x: Failed to acquire 'suspend' inhibitor.\n");
	d->inhibitor_state |= L4X_VBUS_INH_SUSPEND;

	r = L4XV_FN_i(l4re_inhibitor_release(vbus, L4VBUS_INHIBITOR_WAKEUP));
	if (r < 0)
		pr_info("L4x: Failed to release 'wakeup' inhibitor.\n");
	d->inhibitor_state &= ~L4X_VBUS_INH_WAKEUP;
	return 0;
}

static int vbus_root_resume(struct device *dev)
{
	if (device_may_wakeup(dev))
		l4x_event_source_enable_wakeup(&vbus_event_source, false);
	l4x_event_poll_source(&vbus_event_source);
	return 0;
}

static const struct dev_pm_ops l4vbus_root_pm = {
	.suspend = vbus_root_suspend,
	.suspend_noirq = vbus_root_suspend_noirq,
	.resume  = vbus_root_resume,
	.resume_noirq  = vbus_root_resume_noirq,
};

static struct l4x_vbus_driver vbus_root_driver = {
	.driver.name = "l4vbus-root",
	.ops = {
		.add      = vbus_root_add,
		.remove   = vbus_root_remove,
		.shutdown = vbus_root_shutdown,
		.notify   = vbus_root_notify,
	},
	.driver.pm   = &l4vbus_root_pm,
	.driver.bus  = &l4x_vbus_bus_type,
};


static int l4x_vbus_match(struct device *dev, struct device_driver *drv)
{
	struct l4x_vbus_driver *driver = l4x_vbus_driver_from_driver(drv);
	struct l4x_vbus_device *device = l4x_vbus_device_from_device(dev);
	const struct l4x_vbus_device_id *id;

	for (id = driver->id_table; id && id->cid; ++id)
		if (!strcmp(device->hid, id->cid))
			return 1;

	if (device->vbus_handle == L4VBUS_ROOT_BUS && driver == &vbus_root_driver)
		return 1;

	return 0;
}

static int l4x_vbus_probe(struct device *dev)
{
	struct l4x_vbus_device *device = l4x_vbus_device_from_device(dev);
	struct l4x_vbus_driver *driver = l4x_vbus_driver_from_driver(dev->driver);
	int res;

	if (!driver->ops.add)
		return -ENOSYS;

	res = driver->ops.add(device);
	if (res < 0)
		return res;

	if (driver->ops.notify) {
		device->ev_stream.id = device->vbus_handle;
		res = l4x_event_source_register_stream(&vbus_event_source,
		                                       &device->ev_stream);
		if (res < 0)
			return res;
	}

	return 0;
}

static int l4x_vbus_remove(struct device *dev)
{
	struct l4x_vbus_device *device = l4x_vbus_device_from_device(dev);
	if (dev->driver) {
		struct l4x_vbus_driver *driver;
		driver = l4x_vbus_driver_from_driver(dev->driver);

		if (driver->ops.notify)
			l4x_event_source_unregister_stream(&vbus_event_source,
							   &device->ev_stream);

		if (driver->ops.remove)
			driver->ops.remove(device);
	}

	device->driver_data = NULL;
	put_device(dev);
	return 0;
}

static void l4x_vbus_shutdown(struct device *dev)
{
	struct l4x_vbus_device *device = l4x_vbus_device_from_device(dev);
	struct l4x_vbus_driver *driver;
	if (!dev->driver)
		return;
	driver = l4x_vbus_driver_from_driver(dev->driver);

	if (driver->ops.notify)
		l4x_event_source_unregister_stream(&vbus_event_source,
						   &device->ev_stream);

	if (driver->ops.shutdown)
		driver->ops.shutdown(device);
}

static const struct dev_pm_ops l4vbus_pm = {
	.suspend	= pm_generic_suspend,
	.resume		= pm_generic_resume,
	.freeze		= pm_generic_freeze,
	.thaw		= pm_generic_thaw,
	.poweroff	= pm_generic_poweroff,
	.restore	= pm_generic_restore,
};

struct bus_type l4x_vbus_bus_type = {
	.name   = "l4vbus",
	.match  = l4x_vbus_match,
	.probe  = l4x_vbus_probe,
	.remove = l4x_vbus_remove,
	.shutdown = l4x_vbus_shutdown,
	.pm     = &l4vbus_pm,
};

static int vbus_scan(struct device *parent, l4vbus_device_handle_t parent_hdl);

static int create_vbus_device(struct device *parent,
                              l4vbus_device_handle_t handle,
                              char const *name,
                              struct l4x_vbus_device **rdev)
{
	int err;
	unsigned i;
	struct l4x_vbus_device *dev;
	l4vbus_device_t devinfo;

	dev = kzalloc(sizeof(struct l4x_vbus_device), GFP_KERNEL);
	if (!dev) {
		pr_err("l4vbus: cannot allocate device\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&dev->resources);

	dev->vbus_handle = handle;
	dev->dev.parent = parent;
	dev->dev.bus = &l4x_vbus_bus_type;

	err = L4XV_FN_i(l4vbus_get_hid(vbus, handle,
	                               dev->hid, sizeof(dev->hid)));
	if (err < 0) {
		pr_warn("l4vbus: cannot get HID for device %s:%lx: err=%d\n",
		        name, handle, err);
		dev->hid[0] = 0;
	} else
		dev->hid[sizeof(dev->hid) - 1] = 0;

	dev_set_name(&dev->dev, "%s:%s:%lx", dev->hid, name, handle);

	/* get all device resources */
	err = L4XV_FN_i(l4vbus_get_device(vbus, handle, &devinfo));
	if (err < 0) {
		dev_err(&dev->dev, "getting device info: %d\n", err);
		return err;
	}

	for (i = 0; i < devinfo.num_resources; ++i) {
		struct l4x_vbus_resource *vbus_res;
		l4vbus_resource_t res;
		size_t sz = sizeof(res.id);
		char *n;
		err = L4XV_FN_i(l4vbus_get_resource(vbus, handle, i, &res));
		if (err < 0) {
			dev_warn(&dev->dev, "could not request resource\n");
			continue;
		}

		vbus_res = kzalloc(sizeof(struct l4x_vbus_resource),
		                   GFP_KERNEL);
		if (!vbus_res) {
			err = -ENOMEM;
			break;
		}

		n = kmalloc(sz + 1, GFP_KERNEL);
		if (n) {
			strncpy(n, (char *)&res.id, sz);
			n[sizeof(res.id)] = '\0';
			vbus_res->res.name = n;
		}

		vbus_res->res.start = res.start;
		vbus_res->res.end = res.end;

		switch (res.type) {
			case L4VBUS_RESOURCE_MEM:
				vbus_res->res.flags = IORESOURCE_MEM;
				break;
			case L4VBUS_RESOURCE_PORT:
				vbus_res->res.flags = IORESOURCE_IO;
				break;
			case L4VBUS_RESOURCE_IRQ:
				vbus_res->res.flags = IORESOURCE_IRQ;
				break;
			case L4VBUS_RESOURCE_BUS:
				vbus_res->res.flags = IORESOURCE_BUS;
				break;
			case L4VBUS_RESOURCE_GPIO:
			case L4VBUS_RESOURCE_DMA_DOMAIN:
				/* ignore these resource types without warning
				 * because they don't have a matching counter
				 * part in Linux anyways */
				break;
			default:
				dev_warn(&dev->dev, "unknown resource type, "
					 "ignoring\n");
				break;
		}

		list_add_tail(&vbus_res->list, &dev->resources);
	}

	if (err < 0) {
		dev_err(&dev->dev, "getting device resources: %d\n", err);
		return err;
	}

	if ((err = device_register(&dev->dev)) < 0) {
		pr_err("l4vbus: cannot register device\n");
		kfree(dev);
		return err;
	}

	if (rdev) {
		*rdev = dev;
		return 0;
	}

	vbus_scan(&dev->dev, handle);
	return 0;
}

static int vbus_scan(struct device *parent, l4vbus_device_handle_t parent_hdl)
{
	l4vbus_device_handle_t cur = L4VBUS_NULL;
	l4vbus_device_t devinfo;

	while (!L4XV_FN_i(l4vbus_get_next_device(vbus, parent_hdl,
	                                         &cur, 0, &devinfo))) {
		int err = create_vbus_device(parent, cur, devinfo.name, 0);
		if (err)
			pr_err("l4vbus: could not create device %lx: %d\n",
			       cur, err);
	}
	return 0;
}

/**
 * Get a device resource of a specific resource type
 *
 * @param dev   Device to get the resource from.
 * @param type  The requested resource type.
 * @param num   Resource number, counting starts at 0.
 *
 * @retval != NULL  Pointer to the device resource.
 * @retval NULL     No resource found.
 */
struct resource *
l4x_vbus_device_get_resource(struct l4x_vbus_device *dev,
                             unsigned long type, unsigned num)
{
	struct l4x_vbus_resource *vbus_res;
	struct resource *res;

	list_for_each_entry(vbus_res, &dev->resources, list) {
		res = &vbus_res->res;
		if (resource_type(res) == type && num-- == 0)
			return res;
	}

	return NULL;
}
EXPORT_SYMBOL(l4x_vbus_device_get_resource);

static void l4lx_initiate_suspend(struct work_struct *work)
{
	pm_suspend(PM_SUSPEND_MEM);
}

static void l4lx_initiate_shutdown(struct work_struct *work)
{
	kernel_power_off();
}

/**
 * This function is called during a kernel panic. It ensures all inhibitor locks
 * at IO are freed, and the system can still suspend.
 */
static int
vbus_panic(struct notifier_block *self, unsigned long v, void *p)
{
	(void)self;
	(void)v;
	(void)p;
	vbus_root_shutdown(vbus_root);
	return NOTIFY_DONE;
}

static struct notifier_block vbus_panic_notifier = {
	.notifier_call = vbus_panic
};

static int __init vbus_init(void)
{
	int err;

	err = bus_register(&l4x_vbus_bus_type);
	if (err < 0)
		return err;

	vbus = l4re_env_get_cap("vbus");
	if (l4_is_invalid_cap(vbus))
		return -ENOENT;

	pr_info("l4vbus: is running\n");

	vbus_event_source.event_cap = vbus;
	l4x_event_setup_source(&vbus_event_source, 1 << 8, &vbus_event_ops);

	INIT_WORK(&w_suspend, l4lx_initiate_suspend);
	INIT_WORK(&w_shutdown, l4lx_initiate_shutdown);

	err = driver_register(&vbus_root_driver.driver);
	create_vbus_device(NULL, 0, "l4vbus-root", &vbus_root);

	atomic_notifier_chain_register(&panic_notifier_list,
	                               &vbus_panic_notifier);

	return err;
}

subsys_initcall(vbus_init);

static int __init vbus_devs_init(void)
{
	if (vbus_root)
		vbus_scan(&vbus_root->dev, vbus_root->vbus_handle);
	return 0;
}

device_initcall(vbus_devs_init);
