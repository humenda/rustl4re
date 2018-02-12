#ifndef __ASM_L4__GENERIC__L4X_VBUS_H__
#define __ASM_L4__GENERIC__L4X_VBUS_H__

#include <linux/device.h>

#include <asm/generic/event.h>

enum {
	L4X_DEVICE_MAX_HID_LEN = 32,
};

extern struct bus_type l4x_vbus_bus_type;

struct l4x_vbus_device_id {
	const char *cid;
};

struct l4x_vbus_resource {
	struct list_head list;
	struct resource res;
};

struct l4x_vbus_device {
	unsigned long vbus_handle;
	char hid[L4X_DEVICE_MAX_HID_LEN];
	void *driver_data;
	struct device dev;
	struct l4x_event_stream ev_stream;

	struct list_head resources;
};

struct l4x_vbus_driver_ops {
	int (*add)(struct l4x_vbus_device *dev);
	int (*remove)(struct l4x_vbus_device *dev);
	void (*shutdown)(struct l4x_vbus_device *dev);
	void (*notify)(struct l4x_vbus_device *dev, unsigned type,
	               unsigned code, unsigned value);
};

struct l4x_vbus_driver {
	const struct l4x_vbus_device_id *id_table;
	struct l4x_vbus_driver_ops ops;
	struct device_driver driver;
};

struct resource *
l4x_vbus_device_get_resource(struct l4x_vbus_device *dev,
                             unsigned long type, unsigned num);

static inline struct l4x_vbus_device *
l4x_vbus_device_from_device(struct device *dev)
{
	return container_of(dev, struct l4x_vbus_device, dev);
}

static inline struct l4x_vbus_device *
l4x_vbus_device_from_stream(struct l4x_event_stream *stream)
{
	return container_of(stream, struct l4x_vbus_device, ev_stream);
}

static inline struct l4x_vbus_driver *
l4x_vbus_driver_from_driver(struct device_driver *drv)
{
	return container_of(drv, struct l4x_vbus_driver, driver);
}

static inline int l4x_vbus_register_driver(struct l4x_vbus_driver *drv)
{
	drv->driver.bus = &l4x_vbus_bus_type;
	return driver_register(&drv->driver);
}

static inline void l4x_vbus_unregister_driver(struct l4x_vbus_driver *drv)
{
	driver_unregister(&drv->driver);
}

#define module_l4x_vbus_driver(__DRV) \
	module_driver(__DRV, l4x_vbus_register_driver, \
	              l4x_vbus_unregister_driver)

#endif
