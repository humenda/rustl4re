/*
 * rtc.c: Stub driver for rtc service.
 *
 * (C) Copyright 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 * (C) Copyright 2015 Adam Lackorzynski <adam@l4re.org>
 *
 * *NOTE* The time is always comunicated as the offset of real time
 * to the CPU's timestamp counter (TSC). Therefore it will return false results
 * when the TSC is not synchronized across CPUs and the communication between
 * the rtc server and this driver runs across CPU boundaries.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This driver is based upon drivers/rtc/rtc-mrst.c
 */
#define pr_fmt(fmt) "rtc-l4x: " fmt

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/mc146818rtc.h>

#include <asm/generic/util.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/l4lib.h>

#ifdef CONFIG_X86
#include <asm/x86_init.h>
#endif

#include <l4/sys/factory.h>
#include <l4/sys/task.h>
#include <l4/sys/icu.h>
#include <l4/rtc/rtc.h>
#include <l4/re/env.h>

static struct rtc_device *rtc_dev;
static l4_cap_idx_t rtc_server = L4_INVALID_CAP, irq_cap;
static int irq;

static char driver_name[] = "rtc-l4x";

/**
 * The offset between the CPU timestamp counter and the system's rtc clock in
 * nanoseconds.
 */
static u64 offset_to_realtime;

static struct work_struct w_update_time;

L4_EXTERNAL_FUNC(l4rtc_get_offset_to_realtime);
L4_EXTERNAL_FUNC(l4rtc_set_offset_to_realtime);
L4_EXTERNAL_FUNC(l4rtc_get_timer);

static int l4x_rtc_update_offset(void)
{
	if (L4XV_FN_i(l4rtc_get_offset_to_realtime(rtc_server,
	                                           &offset_to_realtime))) {
		pr_err("l4x-rtc: Failed getting time offset.\n");
		return -ENOSYS;
	}
	return 0;
}

static void l4x_rtc_update_time(struct work_struct *work)
{
	struct timespec ts;
	u64 current_time;

	l4x_rtc_update_offset();
	current_time = offset_to_realtime + l4rtc_get_timer();

	ts.tv_nsec = do_div(current_time, 1000000000);
	ts.tv_sec = current_time;

	do_settimeofday(&ts);
}

static irqreturn_t l4x_rtc_int(int irq, void *data)
{
	schedule_work(&w_update_time);
	return 0;
}

static int l4x_rtc_read_time(struct device *dev, struct rtc_time *time)
{
	u64 current_time;
	unsigned long flags;

	current_time = offset_to_realtime + l4rtc_get_timer();

	spin_lock_irqsave(&rtc_lock, flags);
	do_div(current_time, 1000000000);
	rtc_time_to_tm(current_time, time);
	spin_unlock_irqrestore(&rtc_lock, flags);

	return rtc_valid_tm(time);
}

static int l4x_rtc_set_time(struct device *dev, struct rtc_time *time)
{
	u64 new_offset_to_realtime;
	long unsigned int seconds;

	rtc_tm_to_time(time, &seconds);
	new_offset_to_realtime = (u64)seconds * 1000000000 - l4rtc_get_timer();
	if (L4XV_FN_i(l4rtc_set_offset_to_realtime(rtc_server,
	                                           new_offset_to_realtime))) {
		pr_err("l4x-rtc: Could not set time.\n");
		return -EAGAIN;
	}
	/*
	 * We do not update our internal offset to realtime here. It will be
	 * updated when the rtc server sends its "time changed" IRQ.
	 */
	return 0;
}

static const struct rtc_class_ops l4x_rtc_ops = {
	.read_time = l4x_rtc_read_time,
	.set_time  = l4x_rtc_set_time,
};

#ifdef	CONFIG_PM
static int l4x_rtc_resume(struct device *dev)
{
	l4x_rtc_update_offset();
	return 0;
}
#else
#define	l4x_rtc_resume	NULL
#endif

static int l4x_rtc_platform_probe(struct platform_device *pdev)
{
	int r;

	if (l4x_re_resolve_name("rtc", &rtc_server)) {
		pr_err("l4x-rtc: Could not find 'rtc' cap.\n");
		return -ENOENT;
	}

	irq_cap = l4x_cap_alloc();
	if (l4_is_invalid_cap(irq_cap)) {
		pr_err("l4x-rtc: Could not allocate irq cap.\n");
		return -ENOMEM;
	}

	if (L4XV_FN_e(l4_factory_create_irq(l4re_env()->factory, irq_cap))) {
		pr_err("l4x-rtc: Could not create user irq.\n");
		r = -ENOMEM;
		goto free_cap;
	}

	if (L4XV_FN_e(l4_icu_bind(rtc_server, 0, irq_cap))) {
		pr_err("l4x-rtc: Error registering for time updates.\n");
		r = -ENOSYS;
		goto free_irq_cap;
	}

	irq = l4x_register_irq(irq_cap);
	if (irq < 0) {
		pr_err("l4x-rtc: Error registering IRQ with L4Linux.\n");
		r = irq;
		goto free_irq_cap;
	}

	r = request_irq(irq, l4x_rtc_int, IRQF_TRIGGER_RISING, "l4x_rtc", NULL);
	if (r) {
		pr_err("l4x-rtc: Could not register IRQ.\n");
		goto unregister_irq;
	}

	if (l4x_rtc_update_offset()) {
		pr_err("l4x-rtc: Could not get the time offset to real time.\n");
		r = -ENOSYS;
		goto free_irq;
	}

	rtc_dev = rtc_device_register(driver_name, &(pdev->dev),
	                              &l4x_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc_dev)) {
		pr_err("l4x-rtc: Could not register as rtc device.\n");
		r = PTR_ERR(rtc_dev);
		goto free_irq;
	}

	INIT_WORK(&w_update_time, l4x_rtc_update_time);

	return 0;

free_irq:
	free_irq(irq, NULL);
unregister_irq:
	l4x_unregister_irq(irq);
free_irq_cap:
	L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP, irq_cap));
free_cap:
	l4x_cap_free(irq_cap);
	return r;
}

static int l4x_rtc_platform_remove(struct platform_device *pdev)
{
	rtc_device_unregister(rtc_dev);
	free_irq(irq, NULL);
	l4x_unregister_irq(irq);
	L4XV_FN_v(l4_task_delete_obj(L4RE_THIS_TASK_CAP, irq_cap));
	l4x_cap_free(irq_cap);
	return 0;
}

MODULE_ALIAS("platform:l4x_rtc");

static struct dev_pm_ops l4x_rtc_pm = {
	.resume  = l4x_rtc_resume,
};

static struct platform_driver l4x_rtc_platform_driver = {
	.probe		= l4x_rtc_platform_probe,
	.remove		= l4x_rtc_platform_remove,
	.driver = {
		.name   = driver_name,
		.pm     = &l4x_rtc_pm,
	}
};

static struct platform_device l4x_rtc_device = {
	.name = driver_name,
	.id   = -1,
};

static int __init l4x_init_rtc(void)
{
	int ret;
	ret = platform_driver_register(&l4x_rtc_platform_driver);
	if (ret) {
		pr_err("l4rtc: platform_driver_register failed '%d'\n", ret);
		return ret;
	}

	ret = platform_device_register(&l4x_rtc_device);
	if (ret) {
		pr_err("l4rtc: platform_device_register failed '%d'\n", ret);
		platform_driver_unregister(&l4x_rtc_platform_driver);
	}

	return ret;
}

static void __exit l4x_exit_rtc(void)
{
	platform_device_unregister(&l4x_rtc_device);
	platform_driver_unregister(&l4x_rtc_platform_driver);
}

module_init(l4x_init_rtc);
module_exit(l4x_exit_rtc);

MODULE_AUTHOR("Steffen Liebergeld");
MODULE_DESCRIPTION("Stub driver for RTC service");
MODULE_LICENSE("GPL");
