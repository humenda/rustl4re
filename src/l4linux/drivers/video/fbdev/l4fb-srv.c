/*
 * (c) Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See file COPYING-GPL-2 for details.
 */
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/module.h>

#include <asm/generic/l4lib.h>
#include <asm/generic/vcpu.h>

#include <asm/server/fb-srv.h>

#define PREFIX "l4fb-srv: "

L4_EXTERNAL_FUNC(l4x_fb_srv_set);

static int enable;
static int lock_console;
static int running;

static struct l4fb_info info;
static struct fb_info *fb_info; /* fb_info of chosen framebuffer */

/**
 * get_framebuffer
 *
 * Tries to find a Linux framebuffer, collect its information and convey it
 * to the Fb object implemented in arch/l4/server/fb-srv.cc. Afterwards it
 * can optionally grab the console lock to ensure that nothing except us
 * will use the framebuffer.
 *
 * Called on initialization and in response to a "framebuffer registered"
 * event.
 *
 * Return 0 on success, negative error code otherwise.
 */
static int get_framebuffer(void)
{
	int info_idx = -1, ret = -1;
	const struct fb_videomode *mode;
	struct fb_var_screeninfo var;

	if (running) /* support only one framebuffer at a time */
		return 0;

	if (num_registered_fb) {
		int i;
		for (i = 0; i < FB_MAX; i++) {
			if (registered_fb[i]) {
				info_idx = i;
				break;
			}
		}
	}

	if (info_idx < 0 || registered_fb[info_idx] == NULL)
		return -ENOENT;

	pr_info(PREFIX "using framebuffer number %d\n", info_idx);

	fb_info = registered_fb[info_idx];

	/* sanity check */
	if (!fb_info->fbops) {
		pr_err(PREFIX "something is wrong with the framebuffer\n");
		return -ENODEV;
	}

	/* If the framebuffer was created by a module ensure the module
	   cannot be unloaded while we access it. */
	if (!try_module_get(fb_info->fbops->owner)) {
		pr_err(PREFIX "failed to lock framebuffer module.\n");
		return -ENODEV;
	}

	var = fb_info->var;

	if (fb_info->fbops->fb_open) {
		ret = fb_info->fbops->fb_open(fb_info, 0);
		if (ret)
			pr_err(PREFIX "error fb_open code %d\n", ret);
	}

	if (fb_info->fbops->fb_set_par) {
		ret = fb_info->fbops->fb_set_par(fb_info);
		if (ret)
			pr_err(PREFIX "detected "
			       "unhandled fb_set_par error, "
			       "error code %d\n", ret);
	}

	if (fb_info->state != FBINFO_STATE_RUNNING) {
		pr_err(PREFIX "framebuffer %s does not seem to be running.\n",
		       fb_info->fix.id);
		return -ENODEV;
	}
	running = 1;

	mode = fb_find_best_mode(&var, &fb_info->modelist);
	if (mode == NULL) {
		pr_err(PREFIX "did not find a video mode\n");
		return -EINVAL;
	}

	pr_info(PREFIX "using video mode %d:%d\n", mode->xres, mode->yres);

	fb_videomode_to_var(&var, mode);
	var.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
	ret = fb_set_var(fb_info, &var);
	if (ret < 0) {
		pr_err(PREFIX "could not activate video mode\n");
		return ret;
	}

#define F fb_info->var
	info.i.width                      = F.xres;
	info.i.height                     = F.yres;
	info.i.pixel_info.bytes_per_pixel = (F.bits_per_pixel + 7) / 8;
	info.i.pixel_info.r.shift         = F.red.offset;
	info.i.pixel_info.r.size          = F.red.length;
	info.i.pixel_info.g.shift         = F.green.offset;
	info.i.pixel_info.g.size          = F.green.length;
	info.i.pixel_info.b.shift         = F.blue.offset;
	info.i.pixel_info.b.size          = F.blue.length;
	info.i.pixel_info.a.shift         = F.transp.offset;
	info.i.pixel_info.a.size          = F.transp.length;
#undef F

	info.screen_base        = (l4_addr_t)fb_info->screen_base;
	info.screen_size        = (l4_addr_t)fb_info->fix.smem_len;
	info.bytes_per_scanline = fb_info->fix.line_length;
	if (info.screen_size == 0) {
		/* in the case of a useless screen size calculate the size
		 * from the bytes per line
		 */
		info.screen_size = l4_round_page(info.bytes_per_scanline * info.i.height);
	}
	pr_debug(PREFIX "screen_base: 0x%lx\n", info.screen_base);
	pr_debug(PREFIX "screen_size: 0x%lx\n", info.screen_size);
	pr_debug(PREFIX "bytes_per_scanline = %d bitsPP=%d\n", info.bytes_per_scanline, fb_info->var.bits_per_pixel);
	pr_debug(PREFIX "smem_start = 0x%lx\n", fb_info->fix.smem_start);

	/* now tell it L4Re */
	L4XV_FN_v(l4x_fb_srv_set(&info, l4x_cpu_thread_get_cap(0)));

	/* optionally ensure that nothing can interfere
	 * (this disables all linux consoles) */
	if (lock_console) {
		pr_info(PREFIX "grabbing console lock.\n");
		console_lock();
	}

	return 0;
}

/*
 * react to framebuffer events
 * Currently react to new framebuffers only.
 * Leave the other cases here for future reference.
 */
static int l4fb_event_notify(struct notifier_block *self,
			     unsigned long action, void *data)
{
	switch (action) {
	case FB_EVENT_FB_REGISTERED:
		pr_debug(PREFIX "L4 (%s): framebuffer registered\n", __func__);
		get_framebuffer();
		break;
	case FB_EVENT_FB_UNREGISTERED:
		pr_debug(PREFIX "UNIMPLEMENTED: FB_EVENT_FB_UNREGISTERED\n");
		break;
	case FB_EVENT_MODE_CHANGE:
		pr_debug(PREFIX "UNIMPLEMENTED: FB_EVENT_MODE_CHANGE\n");
		break;
	case FB_EVENT_MODE_CHANGE_ALL:
		pr_debug(PREFIX "UNIMPLEMENTED: FB_EVENT_MODE_CHANGE_ALL\n");
		break;
	case FB_EVENT_NEW_MODELIST:
		pr_debug(PREFIX "UNIMPLEMENTED: FB_EVENT_NEW_MODELIST\n");
		break;
	default:
		pr_debug(PREFIX "Unknown framebuffer event.\n");
		return NOTIFY_BAD;
	}
	return NOTIFY_DONE;
}

static struct notifier_block l4fb_event_notifier = {
	.notifier_call = l4fb_event_notify,
};

static int __init l4fb_srv_init(void)
{
	int found_fb = 0;

	if (!enable)
		return 0;

	printk("L4 Framebuffer Service\n");

	/**
	 * Finding a Linux framebuffer:
	 * a) Linux already set up the framebuffer, because the graphics
	 * drivers were loaded and initialized before l4fb-drv.
	 * b) Linux emits a notification when the framebuffer driver calles
	 * register_framebuffer().
	 *
	 * Therefore we first check if a framebuffer is already available.
	 * If it is not, we register for the notification.
	 */
	if (num_registered_fb)
		found_fb = !get_framebuffer();

	if (!found_fb)
		fb_register_client(&l4fb_event_notifier);

	return 0;
}

static void __exit l4fb_srv_exit(void)
{
	/* tell linux we are no longer dependant on the code providing
	 * the framebuffer */
	if (lock_console)
		console_unlock();

	if (fb_info)
		module_put(fb_info->fbops->owner);
}

module_init(l4fb_srv_init);
module_exit(l4fb_srv_exit);

module_param(enable, int, 0444);
MODULE_PARM_DESC(enable, "Enable framebuffer server");

module_param(lock_console, int, 0444);
MODULE_PARM_DESC(lock_console, "Grab console lock to ensure that nothing can interfere with l4fb-srv");

MODULE_AUTHOR("Steffen Liebergeld");
MODULE_DESCRIPTION("L4 framebuffer server");
MODULE_LICENSE("GPL");
