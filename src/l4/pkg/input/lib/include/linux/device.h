#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <linux/types.h>
#include <linux/errno.h>
#include <asm/semaphore.h>

struct module;

static inline struct class_simple *class_simple_create(struct module *owner, char *name)
{
	return NULL;
}

#define class_simple_destroy(x)
#define device_create_file(d,e)
#define device_remove_file(d,a)

struct bus_type {
	char		* name;
};

struct device {
	struct device	* parent;
	struct device_driver *driver;
	char   bus_id[20];
	struct bus_type * bus;

	void   (*release)(struct device *dev);
};

struct device_driver {
	char		* name;
	struct bus_type	* bus;
};

struct platform_device {
	char		* name;
	u32		id;
	struct device	dev;
	u32		num_resources;
	struct resource	* resource;
};

#define get_driver(x)
#define put_driver(x)
#define device_bind_driver(x)
#define driver_unregister(x)
#define platform_device_unregister(x)

static int inline driver_register(struct device_driver *drv)
{
	return 0;
}

#endif
