/*
 * Framebuffer driver
 *
 * based on vesafb.c
 *
 * Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/screen_info.h>
#include <asm/uaccess.h>
#include <linux/bitops.h>

#include <asm/generic/l4lib.h>
#include <l4/sys/err.h>
#include <l4/sys/task.h>
#include <l4/sys/factory.h>
#include <l4/sys/icu.h>
#include <l4/re/c/util/video/goos_fb.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/event_buffer.h>
#include <l4/re/c/video/view.h>
#include <l4/re/c/video/goos.h>
#include <l4/log/log.h>

#include <asm/l4lxapi/misc.h>
#include <asm/generic/l4fb_ioctl.h>

#include <asm/generic/setup.h>
#include <asm/generic/l4fb.h>
#include <asm/generic/event-input.h>
#include <asm/generic/cap_alloc.h>
#include <asm/generic/util.h>
#include <asm/generic/log.h>
#include <asm/generic/vcpu.h>
#ifdef CONFIG_L4_FB_DRIVER_DBG
#include <asm/generic/stats.h>
#endif

L4_EXTERNAL_FUNC(l4re_video_goos_delete_view);
L4_EXTERNAL_FUNC(l4re_video_goos_create_view);
L4_EXTERNAL_FUNC(l4re_video_goos_delete_buffer);
L4_EXTERNAL_FUNC(l4re_video_goos_get_view);
L4_EXTERNAL_FUNC(l4re_video_view_get_info);
L4_EXTERNAL_FUNC(l4re_video_view_set_info);
L4_EXTERNAL_FUNC(l4re_video_goos_get_static_buffer);
L4_EXTERNAL_FUNC(l4re_video_goos_info);
L4_EXTERNAL_FUNC(l4re_video_goos_create_buffer);
L4_EXTERNAL_FUNC(l4re_video_goos_refresh);
L4_EXTERNAL_FUNC(l4re_video_view_set_viewport);
L4_EXTERNAL_FUNC(l4re_video_view_stack);

enum {
	MAX_UNMAP_BITS = 32, /* enough for the framebuffer */
};

struct l4fb_unmap_info {
	unsigned int map[MAX_UNMAP_BITS - 1 - L4_PAGESHIFT];
	unsigned int top;
	unsigned int weight;
	l4_fpage_t *flexpages;
};

struct l4fb_view {
	struct list_head next_view;
	l4re_video_view_t view;
	int index;
};

enum {
	INPUT_HASH_BITS    = 5,
	INPUT_HASH_ENTRIES = 1UL << INPUT_HASH_BITS,
};


struct l4fb_screen {
	struct list_head next_screen;

	unsigned long flags;

	unsigned long managed;
	atomic_t refs;

	l4re_video_goos_t goos;
	l4_cap_idx_t fb_cap;

	l4re_video_goos_info_t ginfo;

	l4_addr_t fb_addr, fb_line_length;
	struct list_head views;

	unsigned int refresh_sleep;
	int refresh_enabled;
	struct timer_list refresh_timer;
	struct l4fb_unmap_info unmap_info;
	u32 pseudo_palette[17];

	/* Input event part */
	struct l4x_event_source input;
	struct platform_device platform_device;
};

static LIST_HEAD(l4fb_screens);
static int disable, verbose, touchscreen, verbose_wm;
static char *fbs[10] = { "fb", };
static int num_fbs = 1;
static const unsigned int unmaps_per_refresh = 1;

static inline struct l4fb_screen *l4fb_screen(struct fb_info *info)
{
	return container_of(container_of(info->device,
	                                 struct platform_device, dev),
	                    struct l4fb_screen, platform_device);
}

#ifdef CONFIG_L4_FB_DRIVER_DBG
static struct dentry *debugfs_dir, *debugfs_unmaps, *debugfs_updates;
static unsigned int stats_unmaps, stats_updates;
#endif

/* -- module paramter variables ---------------------------------------- */

static int refreshsleep = -1;
static int keeprefresh;

/* -- framebuffer variables/structures --------------------------------- */

static struct fb_var_screeninfo const l4fb_defined = {
	.activate	= FB_ACTIVATE_NOW,
	.height		= -1,
	.width		= -1,
	.right_margin	= 32,
	.upper_margin	= 16,
	.lower_margin	= 4,
	.vsync_len	= 4,
	.vmode		= FB_VMODE_NONINTERLACED,
};

static const struct fb_fix_screeninfo l4fb_fix = {
	.id	= "l4fb",
	.type	= FB_TYPE_PACKED_PIXELS,
	.accel	= FB_ACCEL_NONE,
};

/* -- implementations -------------------------------------------------- */


static void l4fb_init_screen(struct l4fb_screen *screen)
{
	screen->goos = L4_INVALID_CAP;

	screen->refresh_sleep = msecs_to_jiffies(100);
	screen->refresh_enabled = 1;

	/* Input event part */
	l4x_event_init_source(&screen->input);

	atomic_set(&screen->refs, 0);
	screen->managed = 0;

	INIT_LIST_HEAD(&screen->views);
}

static void vesa_setpalette(int regno, unsigned red, unsigned green,
			    unsigned blue)
{
}

static int l4fb_setcolreg(unsigned regno, unsigned red, unsigned green,
                          unsigned blue, unsigned transp,
                          struct fb_info *info)
{
	/*
	 *  Set a single color register. The values supplied are
	 *  already rounded down to the hardware's capabilities
	 *  (according to the entries in the `var' structure). Return
	 *  != 0 for invalid regno.
	 */

	if (regno >= info->cmap.len)
		return 1;

	if (info->var.bits_per_pixel == 8)
		vesa_setpalette(regno,red,green,blue);
	else if (regno < 16) {
		switch (info->var.bits_per_pixel) {
		case 16:
			((u32*) (info->pseudo_palette))[regno] =
				((red   >> (16 -   info->var.red.length)) <<   info->var.red.offset) |
				((green >> (16 - info->var.green.length)) << info->var.green.offset) |
				((blue  >> (16 -  info->var.blue.length)) <<  info->var.blue.offset);
			break;
		case 24:
		case 32:
			red   >>= 8;
			green >>= 8;
			blue  >>= 8;
			((u32 *)(info->pseudo_palette))[regno] =
				(red   << info->var.red.offset)   |
				(green << info->var.green.offset) |
				(blue  << info->var.blue.offset);
			break;
		}
	}

	return 0;
}

static int l4fb_pan_display(struct fb_var_screeninfo *var,
                            struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
		    || var->yoffset >= info->var.yres_virtual
		    || var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + var->xres > info->var.xres_virtual ||
		    var->yoffset + var->yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

static void (*l4fb_update_rect)(struct l4fb_screen *, int, int, int, int);

static inline struct device *scr2dev(struct l4fb_screen *screen)
{
	return &screen->platform_device.dev;
}

static void l4fb_delete_all_views(struct l4fb_screen *screen)
{
	struct l4fb_view *view, *tmp;
	list_for_each_entry_safe(view, tmp, &screen->views, next_view) {
		list_del(&view->next_view);
		if (screen->flags & F_l4re_video_goos_dynamic_views)
			L4XV_FN_v(l4re_video_goos_delete_view(screen->goos, &view->view));
		kfree(view);
	}
}

static void l4fb_l4re_update_rect(struct l4fb_screen *screen,
                                  int x, int y, int w, int h)
{
	if (!screen)
		return;

	L4XV_FN_v(l4re_video_goos_refresh(screen->goos, x, y, w, h));
}

static void l4fb_l4re_update_all(struct l4fb_screen *screen)
{
	l4fb_l4re_update_rect(screen, 0, 0,
	                      screen->ginfo.width, screen->ginfo.height);
}

static void l4fb_l4re_update_memarea(struct l4fb_screen *screen,
                                     l4_addr_t base, l4_addr_t size)
{
	int y, h;

	if (size < 0 || base < screen->fb_addr)
		l4x_printf("l4fb: update: WRONG VALS: sz=%ld base=%lx start=%lx\n",
		           size, base, screen->fb_addr);

	y = ((base - screen->fb_addr) / screen->fb_line_length);
	h = (size / screen->fb_line_length) + 1;

#ifdef CONFIG_L4_FB_DRIVER_DBG
	++stats_updates;
#endif

	/* FIXME: assume we have just a single view for now */
	l4fb_l4re_update_rect(screen, 0, y, screen->ginfo.width, h);
}

#if 0
// actually we would need to make a copy of the screen to make is possible
// to restore the before-blank contents on unblank events
static int l4fb_blank(int blank, struct fb_info *info)
{
	// pretent the screen is off
	if (blank != FB_BLANK_UNBLANK)
		memset((void *)info->fix.smem_start, 0, info->fix.smem_len);
	return 0;
}
#endif

static void l4fb_copyarea(struct fb_info *info, const struct fb_copyarea *region)
{
	cfb_copyarea(info, region);
	if (l4fb_update_rect)
		l4fb_update_rect(l4fb_screen(info), region->dx, region->dy, region->width, region->height);
}

static void l4fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	cfb_fillrect(info, rect);
	if (l4fb_update_rect)
		l4fb_update_rect(l4fb_screen(info), rect->dx, rect->dy, rect->width, rect->height);
}

static void l4fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	cfb_imageblit(info, image);
	if (l4fb_update_rect)
		l4fb_update_rect(l4fb_screen(info), image->dx, image->dy, image->width, image->height);
}

static int l4fb_mmap(struct fb_info *info,
                     struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn;

	if (offset + size > info->fix.smem_len)
		return -EINVAL;

	pfn = ((unsigned long)info->fix.smem_start + offset) >> PAGE_SHIFT;
	while (size > 0) {
		if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, PAGE_SHARED)) {
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pfn++;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}
	l4_touch_rw((char *)info->fix.smem_start + offset,
	            vma->vm_end - vma->vm_start);

	return 0;
}

static struct l4fb_view *l4fb_alloc_view(void)
{
	return kzalloc(sizeof(struct l4fb_view), GFP_KERNEL);
}

static struct l4fb_view *l4fb_find_view(struct l4fb_screen *screen, int view)
{
	struct l4fb_view *v;

	list_for_each_entry(v, &screen->views, next_view)
		if (v->index == view)
			return v;
	return NULL;
}


static int l4fb_create_view(struct l4fb_screen *screen, int view)
{
	int ret;
	struct l4fb_view *v;
	l4re_video_view_info_t vi;

	if (verbose_wm)
		dev_info(scr2dev(screen), "create goos view[%d]\n", view);

	if (!(screen->flags & F_l4re_video_goos_dynamic_views))
		return -EINVAL;

	if (l4fb_find_view(screen, view))
		return -EEXIST;

	if (!((v = l4fb_alloc_view())))
		return -ENOMEM;

	v->index = view;
	ret = L4XV_FN_i(l4re_video_goos_create_view(screen->goos, &v->view));

	if (ret < 0) {
		kfree(v);
		return ret;
	}

	/* we use the preferred color mode of the screen */
	vi.pixel_info = screen->ginfo.pixel_info;
	/* we use always a single buffer as backing store */
	vi.buffer_index = 0;
	/* we prepare for classical overlay wm, so all views use
	 * a single scan-line length */
	vi.bytes_per_line = screen->fb_line_length;

	vi.flags =   F_l4re_video_view_set_buffer
	           | F_l4re_video_view_set_bytes_per_line
	           | F_l4re_video_view_set_pixel;


	ret = L4XV_FN_i(l4re_video_view_set_info(&v->view, &vi));
	if (ret)
		dev_err(scr2dev(screen), "error setting the view properties (%d)\n", ret);

	list_add(&v->next_view, &screen->views);
	return 0;
}


static int l4fb_delete_view(struct l4fb_screen *screen, int view)
{
	struct l4fb_view *v = l4fb_find_view(screen, view);

	if (verbose_wm)
		dev_info(scr2dev(screen), "delete goos view[%d]\n", view);

	if (!v)
		return -ENOENT;

	list_del(&v->next_view);
	return L4XV_FN_i(l4re_video_goos_delete_view(screen->goos, &v->view));
}

static int l4fb_background_view(struct l4fb_screen *screen, int view)
{
	struct l4fb_view *v = l4fb_find_view(screen, view);
	l4re_video_view_info_t vi;

	if (verbose_wm)
		dev_info(scr2dev(screen), "background goos view[%d]\n", view);

	if (!v)
		return -ENOENT;

	vi.flags = F_l4re_video_view_set_background;
	return L4XV_FN_i(l4re_video_view_set_info(&v->view, &vi));
}

static int l4fb_place_view(struct l4fb_screen *screen, int view, int x, int y, int w, int h)
{
	struct l4fb_view *v = l4fb_find_view(screen, view);
	unsigned long buffer_offset;

	if (0)
		dev_info(scr2dev(screen), "place goos view[%d] -> %d,%d - %d,%d\n",
		         view, x, y, x + w, y + h);

	if (!v)
		return -ENOENT;

	if (x < 0) {
		w += x;
		x = 0;
	}

	if (w < 0)
		w = 0;

	if (y < 0) {
		h += y;
		y = 0;
	}

	if (h < 0)
		h = 0;

	buffer_offset = y * screen->fb_line_length
	                + x * screen->ginfo.pixel_info.bytes_per_pixel;
	if (0)
		dev_info(scr2dev(screen), "place goos view[%d] -> %d,%d - %d,%d %lu\n", view, x, y,
		         x + w, y + h, buffer_offset);
	return L4XV_FN_i(l4re_video_view_set_viewport(&v->view, x, y, w, h,
	                                              buffer_offset));
}

static int l4fb_stack_view(struct l4fb_screen *screen, int view, int pivot, int behind)
{
	struct l4fb_view *v, *p = NULL;

	if (0)
		dev_info(scr2dev(screen), "stack goos view[%d] %s(%d) view: %d\n", view,
		         behind ? "behind" : "before", behind, pivot);

	v = l4fb_find_view(screen, view);
	if (!v)
		return -ENOENT;

	if (pivot >= 0)
		p = l4fb_find_view(screen, pivot);

	return L4XV_FN_i(l4re_video_view_stack(&v->view, p ? &p->view : NULL, behind));
}

static int l4fb_view_set_flags(struct l4fb_screen *screen, int view, unsigned long flags)
{
	struct l4fb_view *v;
	l4re_video_view_info_t vi;

	v = l4fb_find_view(screen, view);
	if (!v)
		return -ENOENT;

	vi.flags = F_l4re_video_view_set_flags
	           | (flags & F_l4re_video_view_flags_mask);

	return L4XV_FN_i(l4re_video_view_set_info(&v->view, &vi));
}


static int l4fb_ioctl(struct fb_info *info, unsigned int cmd,
                          unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct l4fb_screen *scr = l4fb_screen(info);
	int ret = 0;
	int managed;

	switch (cmd) {
	case L4FB_IOCTL_CREATE_VIEW:
		managed = test_and_set_bit(2, &scr->managed);
		if (!managed)
			l4fb_delete_all_views(scr);
		ret = l4fb_create_view(scr, (int)arg);
		break;
	case L4FB_IOCTL_DESTROY_VIEW:
		ret = l4fb_delete_view(scr, (int)arg);
		break;
	case L4FB_IOCTL_VIEW_SET_FLAGS:
		{
			struct l4fb_view_flags v;
			if (copy_from_user(&v, argp, sizeof(v)))
				return -EFAULT;
			ret = l4fb_view_set_flags(scr, v.view, v.flags);
			break;
		}
		break;
	case L4FB_IOCTL_BACK_VIEW:
		ret = l4fb_background_view(scr, (int)arg);
		break;
	case L4FB_IOCTL_PLACE_VIEW:
		{
			struct l4fb_set_viewport v;
			if (copy_from_user(&v, argp, sizeof(v)))
				return -EFAULT;
			ret = l4fb_place_view(scr, v.view, v.r.x, v.r.y, v.r.w, v.r.h);
			break;
		}
	case L4FB_IOCTL_STACK_VIEW:
		{
			struct l4fb_stack_view v;
			if (copy_from_user(&v, argp, sizeof(v)))
				return -EFAULT;
			ret = l4fb_stack_view(scr, v.view, v.pivot, v.behind);
			break;
		}
	case L4FB_IOCTL_REFRESH:
		{
			struct l4fb_region v;
			if (copy_from_user(&v, argp, sizeof(v)))
				return -EFAULT;
			l4fb_l4re_update_rect(scr, v.x, v.y, v.w, v.h);

			if (!keeprefresh)
				scr->refresh_sleep = 0;
			break;
		}
	default:
		dev_info(scr2dev(scr), "unknown ioctl: %x\n", cmd);
		ret = -EINVAL;
		break;
	}
	if (ret)
		dev_info(info->device, "ioctl ret=%d\n", ret);
	return ret;
}

static int create_main_view(struct l4fb_screen *screen, struct fb_info *info)
{
	/* create a full-screen view for the kernel console */
	int res = l4fb_create_view(screen, -4 /* special kernel view ID */);
	if (res < 0)
		return res;
	l4fb_place_view(screen, -4, 0, 0, info->var.xres, info->var.yres);
	l4fb_stack_view(screen, -4, 0, 0);
	return 0;
}

static int l4fb_open(struct fb_info *info, int user)
{
	struct l4fb_screen *screen = l4fb_screen(info);
	int r;
	if (verbose)
		dev_info(scr2dev(screen), "opening frame-buffer device for %s\n",
		         user ? "user" : "kernel");

	r = atomic_add_return(1, &screen->refs);

	if (r == 1 && screen->flags & F_l4re_video_goos_dynamic_views)
		return create_main_view(screen, info);

	return 0;
}

static int l4fb_release(struct fb_info *info, int user)
{
	struct l4fb_screen *screen = l4fb_screen(info);
	int r, user_managed;
	/* hm, we just throw away all the dynamic view inforamtion here
	 * May be, it is better to only clean up when all clients did a
	 * release?.
	 */
	if (verbose)
		dev_info(scr2dev(screen), "releasing frame-buffer device for %s\n",
		         user ? "user" : "kernel");

	r = atomic_sub_return(1, &screen->refs);
	user_managed = test_and_clear_bit(2, &screen->managed);

	/* Nothing to cleanup for static goos sessions. */
	if (!(screen->flags & F_l4re_video_goos_dynamic_views))
		return 0;

	if (verbose_wm)
		dev_info(scr2dev(screen), "cleanup goos stuff\n");

	if (user_managed) {
		l4fb_delete_all_views(screen);
		if (r)
			create_main_view(screen, info);
	}

	return 0;
}


#if 0
/* some try to figure aout VT switching from and to X-Server */
static int l4fb_check_var(struct fb_var_screeninfo *var,
                          struct fb_info *info)
{
	*var = info->var;
	return 0;
}

static int l4fb_set_par(struct fb_info *info)
{
	dev_info(info->device, "set par ... %lx %lx %lx\n",
	         info->flags, info->var.vmode, info->var.activate);
	//init your hardware here
	return 0;
}
#endif

static struct fb_ops l4fb_ops = {
	.owner		= THIS_MODULE,
	.fb_open        = l4fb_open,
	.fb_release     = l4fb_release,
	.fb_setcolreg	= l4fb_setcolreg,
	.fb_pan_display	= l4fb_pan_display,
	.fb_fillrect	= l4fb_fillrect,
	.fb_copyarea	= l4fb_copyarea,
	.fb_imageblit	= l4fb_imageblit,
	.fb_mmap	= l4fb_mmap,
	.fb_ioctl	= l4fb_ioctl,
	//.fb_check_var	= l4fb_check_var,
	//.fb_set_par	= l4fb_set_par,
	//.fb_blank	= l4fb_blank,
};

/* ============================================ */

static void l4fb_update_dirty_unmap(struct l4fb_screen *screen)
{
	unsigned int i;
	l4_msg_regs_t *v = l4_utcb_mr_u(l4_utcb());

	i = 0;
	while (i < screen->unmap_info.weight) {
		l4_msgtag_t tag;
		l4_addr_t bulkstart, bulksize;
		unsigned int j, num_flexpages;

		num_flexpages = screen->unmap_info.weight - i >= L4_UTCB_GENERIC_DATA_SIZE - 2
		                ? L4_UTCB_GENERIC_DATA_SIZE - 2
		                : screen->unmap_info.weight - i;

		tag = l4_task_unmap_batch(L4RE_THIS_TASK_CAP,
		                          screen->unmap_info.flexpages + i,
		                          num_flexpages, L4_FP_ALL_SPACES);

		if (l4_error(tag))
			l4x_printf("l4fb: error with l4_task_unmap_batch\n");

		if (0)
			l4x_printf("l4fb: unmapped %d-%d/%d pages\n", i, i + num_flexpages - 1, screen->unmap_info.weight);

#ifdef CONFIG_L4_FB_DRIVER_DBG
		++stats_unmaps;
#endif

		/* redraw dirty pages bulkwise */
		bulksize = 0;
		bulkstart = L4_INVALID_ADDR;
		for (j = 0; j < num_flexpages; j++) {
			l4_fpage_t ret_fp;
			ret_fp.raw = v->mr[2 + j];
			if ((l4_fpage_rights(ret_fp) & L4_FPAGE_W)) {
				if (0)
					l4x_printf("   %d: addr=%lx size=%x was dirty\n",
					           j, l4_fpage_page(ret_fp) << L4_PAGESHIFT,
					           1 << l4_fpage_size(ret_fp));
				if (!bulksize)
					bulkstart = l4_fpage_page(ret_fp) << L4_PAGESHIFT;
				bulksize += 1 << l4_fpage_size(ret_fp);

				continue;
			}
			// we need to flush
			if (bulkstart != L4_INVALID_ADDR)
				l4fb_l4re_update_memarea(screen, bulkstart, bulksize);
			bulksize = 0;
			bulkstart = L4_INVALID_ADDR;
		}
		if (bulkstart != L4_INVALID_ADDR)
			l4fb_l4re_update_memarea(screen, bulkstart, bulksize);

		i += num_flexpages;
	}
}

/* init flexpage array according to map */
static void l4fb_update_dirty_init_flexpages(struct l4fb_screen *screen,
                                             l4_addr_t addr)
{
	unsigned int log2size, i, j;
	unsigned int size;

	size = screen->unmap_info.weight * sizeof(l4_fpage_t);
	if (verbose)
		l4x_printf("l4fb: going to kmalloc(%d) to store flexpages\n",
		           size);
	screen->unmap_info.flexpages = kmalloc(size, GFP_KERNEL);
	log2size = screen->unmap_info.top;
	for (i = 0; i < screen->unmap_info.weight;) {
		for (j = 0;
		     j < screen->unmap_info.map[log2size - L4_PAGESHIFT];
		     ++j) {
			screen->unmap_info.flexpages[i]
			    = l4_fpage(addr, log2size, 0);
			if (verbose)
				l4x_printf("%d %lx - %lx \n",
				           i, addr, addr + (1 << log2size));
			addr += 1 << log2size;
			++i;
		}
		--log2size;
	}
}

/* try to reduce the number of flexpages needed (weight) to num_flexpages */
static void l4fb_update_dirty_init_optimize(struct l4fb_screen *screen,
                                            unsigned int num_flexpages)
{
	struct l4fb_unmap_info *info = &screen->unmap_info;
	while ((num_flexpages > info->weight
	        || info->top > L4_SUPERPAGESHIFT)
	       && info->top >= L4_PAGESHIFT) {
		info->map[info->top - L4_PAGESHIFT] -= 1;
		info->map[info->top - L4_PAGESHIFT - 1] += 2;
		info->weight += 1;
		if (!info->map[info->top - L4_PAGESHIFT])
			info->top -= 1;
	}
	if (verbose)
		l4x_printf("l4fb: optimized on using %d flexpages, "
		           "%d where requested\n",
		           info->weight, num_flexpages);
}

static void l4fb_update_dirty_init(struct l4fb_screen *screen,
                                   l4_addr_t addr, l4_addr_t size)
{
	struct l4fb_unmap_info *info = &screen->unmap_info;
	//unsigned int num_flexpages = (L4_UTCB_GENERIC_DATA_SIZE - 2) * unmaps_per_refresh;
	unsigned int num_flexpages = (3 - 2) * unmaps_per_refresh;
	unsigned int log2size = MAX_UNMAP_BITS - 1;

	memset(info, 0, sizeof(struct l4fb_unmap_info));
	size = l4_round_page(size);

	/* init map with bitlevel number */
	while (log2size >= L4_PAGESHIFT) {
		if (size & (1 << log2size)) {
			info->map[log2size-L4_PAGESHIFT] = 1;
			if (!info->top)
				info->top = log2size;
			info->weight += 1;
		}
		--log2size;
	}
	l4fb_update_dirty_init_optimize(screen, num_flexpages);
	l4fb_update_dirty_init_flexpages(screen, addr);
}


static void l4fb_refresh_func(struct timer_list *t)
{
	struct l4fb_screen *screen = from_timer(screen, t, refresh_timer);
	if (screen->refresh_enabled && l4fb_update_rect) {
		if (1)
			l4fb_update_rect(screen, 0, 0, screen->ginfo.width,
			                 screen->ginfo.height);
		else
			l4fb_update_dirty_unmap(screen);
	}

	if (screen->refresh_sleep)
		mod_timer(&screen->refresh_timer,
		          jiffies + screen->refresh_sleep);
}

/* ============================================ */
static void l4fb_shutdown(struct l4fb_screen *screen)
{
	del_timer_sync(&screen->refresh_timer);

	/* Also do not update anything anymore */
	l4fb_update_rect = NULL;

	l4x_event_shutdown_source(&screen->input);

	L4XV_FN_v(l4re_rm_detach((void *)screen->fb_addr));

	if (screen->unmap_info.flexpages)
		kfree(screen->unmap_info.flexpages);

	l4fb_delete_all_views(screen);

	if (screen->flags & F_l4re_video_goos_dynamic_buffers)
		L4XV_FN_v(l4re_video_goos_delete_buffer(screen->goos, 0));

	if (l4_is_valid_cap(screen->goos)) {
		L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP,
		                              screen->goos));
		l4x_cap_free(screen->goos);
	}

	screen->flags = 0;
	l4fb_init_screen(screen);
}

static int l4fb_init_session(struct fb_info *fb, struct l4fb_screen *screen)
{
	int ret;
	struct fb_var_screeninfo *const var = &fb->var;
	struct fb_fix_screeninfo *const fix = &fb->fix;
	l4re_video_goos_info_t *ginfo = &screen->ginfo;
	l4re_video_view_info_t vinfo;
	void *fb_addr = 0;

	ret = L4XV_FN_i(l4re_video_goos_info(screen->goos, ginfo));
	if (ret) {
		dev_err(scr2dev(screen), "cannot get goos info (%d)\n", ret);
		return ret;
	}

	ret = -ENOMEM;

	/* check for strange combinations */
	if ((ginfo->flags & F_l4re_video_goos_dynamic_buffers) &&
	    !(ginfo->flags & F_l4re_video_goos_dynamic_views)) {
		dev_err(scr2dev(screen),
		        "dynamic buffer + static view is not supported\n");
		return -EINVAL;
	}

	if (!(ginfo->flags & F_l4re_video_goos_dynamic_views)) {
		struct l4fb_view *view = l4fb_alloc_view();
		if (!view)
			return ret;

		ret = L4XV_FN_i(l4re_video_goos_get_view(screen->goos, 0,
		                                         &view->view));
		if (ret) {
			dev_err(scr2dev(screen), "cannot get static view\n");
			kfree(view);
			return ret;
		}

		ret = L4XV_FN_i(l4re_video_view_get_info(&view->view, &vinfo));
		if (ret) {
			dev_err(scr2dev(screen), "cannot get view info\n");
			kfree(view);
			return ret;
		}

		list_add(&view->next_view, &screen->views);

		/* use the view's resolution as reference */
		var->xres = vinfo.width;
		var->yres = vinfo.height;
		fix->line_length = vinfo.bytes_per_line;

		var->red.offset = vinfo.pixel_info.r.shift;
		var->red.length = vinfo.pixel_info.r.size;
		var->green.offset = vinfo.pixel_info.g.shift;
		var->green.length = vinfo.pixel_info.g.size;
		var->blue.offset = vinfo.pixel_info.b.shift;
		var->blue.length = vinfo.pixel_info.b.size;
		var->bits_per_pixel = screen->ginfo.pixel_info.bytes_per_pixel
		                      * 8;
	} else {
		/* in this case the user must allocate views via the ioctls */
		/* use the goos info for the reference size */
		var->xres = ginfo->width;
		var->yres = ginfo->height;
		fix->line_length = ginfo->width
		                   * ginfo->pixel_info.bytes_per_pixel;

		var->red.offset = ginfo->pixel_info.r.shift;
		var->red.length = ginfo->pixel_info.r.size;
		var->green.offset = ginfo->pixel_info.g.shift;
		var->green.length = ginfo->pixel_info.g.size;
		var->blue.offset = ginfo->pixel_info.b.shift;
		var->blue.length = ginfo->pixel_info.b.size;
		var->bits_per_pixel = screen->ginfo.pixel_info.bytes_per_pixel * 8;
	}

	/* We cannot really set (smaller would work) screen paramenters
	 * when using con */
	if (var->bits_per_pixel == 15)
		var->bits_per_pixel = 16;

	if (l4_is_invalid_cap(screen->fb_cap = l4x_cap_alloc()))
		return ret;

	screen->flags |= ginfo->flags;

	/* allocate the fb memory if not static */
	if (ginfo->flags & F_l4re_video_goos_dynamic_buffers) {
		unsigned long size = fix->line_length * ginfo->height;
		size = l4_round_page(size);

		ret = L4XV_FN_i(l4re_video_goos_create_buffer(screen->goos,
		                                              size,
		                                              screen->fb_cap));
		if (ret) {
			dev_err(scr2dev(screen),
			        "cannot allocate fb (%d)\n", ret);
			return ret;
		}
		fix->smem_len = size;
	} else {
		long ret;
		ret = L4XV_FN_i(l4re_video_goos_get_static_buffer(screen->goos,
		                                                  0,
		                                                  screen->fb_cap));
		if (ret >= 0)
			ret = L4XV_FN_i(l4re_ds_size(screen->fb_cap));

		if (ret < 0) {
			dev_err(scr2dev(screen),
			        "cannot get static fb (%ld)\n", ret);
			return ret;
		}

		fix->smem_len = l4_round_page(ret);
	}

	ret = L4XV_FN_i(l4re_rm_attach(&fb_addr, fix->smem_len,
	                               L4RE_RM_SEARCH_ADDR,
	                               screen->fb_cap | L4_CAP_FPAGE_RW,
	                               0, 20));

	if (ret < 0) {
		dev_err(scr2dev(screen), "cannot map fb memory (%d)\n", ret);
		return ret;
	}

	screen->fb_addr = (unsigned long)fb_addr;
	screen->fb_line_length = fix->line_length;
	fix->smem_start = (unsigned long)fb_addr;

	/* currently the virtual fb is equal to the screen */
	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres;

	fix->visual = FB_VISUAL_TRUECOLOR;

	dev_info(scr2dev(screen), "%dx%d@%d %dbypp, size: %d @ %lx\n",
	         var->xres, var->yres, var->bits_per_pixel,
	         screen->ginfo.pixel_info.bytes_per_pixel, fix->smem_len,
	         fix->smem_start);
	dev_info(scr2dev(screen), "%d:%d:%d %d:%d:%d linelen=%d visual=%d\n",
	         var->red.length, var->green.length, var->blue.length,
	         var->red.offset, var->green.offset, var->blue.offset,
	         fix->line_length, fix->visual);

	l4fb_update_dirty_init(screen, fix->smem_start, fix->smem_len);

	timer_setup(&screen->refresh_timer, l4fb_refresh_func, 0);
	if (screen->refresh_sleep) {
		screen->refresh_timer.expires = jiffies
		                                + screen->refresh_sleep;
		add_timer(&screen->refresh_timer);
	}

	return ret;
}

static inline struct l4fb_screen *dev_to_screen(struct device *dev)
{
	return container_of(container_of(dev, struct platform_device, dev),
	                    struct l4fb_screen, platform_device);
}

static ssize_t
do_refresh(struct device *dev, struct device_attribute *attr,
           const char *buf, size_t count)
{
	l4fb_l4re_update_all(dev_to_screen(dev));
	return count;
}
static DEVICE_ATTR(refresh, 0200, NULL, do_refresh);

static ssize_t do_refresh_sleep_read(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
	return sprintf(buf, "%d\n", dev_to_screen(dev)->refresh_sleep);
}

static ssize_t
do_refresh_sleep_write(struct device *dev, struct device_attribute *attr,
                       const char *buf, size_t count)
{
	struct l4fb_screen *screen = dev_to_screen(dev);
	char tmp[12];
	int sz = count > sizeof(tmp) ? sizeof(tmp) : count;

	memcpy(tmp, buf, sz);
	tmp[sz] = 0;
	screen->refresh_sleep = simple_strtoul(tmp, NULL, 0);

	if (screen->refresh_sleep)
		mod_timer(&screen->refresh_timer,
		          jiffies + screen->refresh_sleep);

	return count;
}
static DEVICE_ATTR(refresh_sleep, 0600,
                   do_refresh_sleep_read, do_refresh_sleep_write);

static int
l4fb_setup_input_dev(struct l4x_event_source *source,
                     struct l4x_event_input_stream *input)
{
	enum { NAME_LEN = 20 };
	int err;
	struct l4fb_screen *screen = container_of(source, struct l4fb_screen,
	                                          input);
	struct fb_info *info = platform_get_drvdata(&screen->platform_device);
	char *name;

	if (verbose)
		dev_info(scr2dev(screen), "new input device id=%lx\n",
		         input->stream.id);

	input->simulate_touchscreen = touchscreen;

	err = l4x_event_input_setup_stream_infos(source, input);
	if (err < 0)
		return err;

	name = kzalloc(NAME_LEN, GFP_KERNEL);
	snprintf(name, NAME_LEN, "L4event '%d'", info->node);
	name[NAME_LEN - 1] = 0;

	input->input->phys = "L4Re::Event";
	input->input->uniq = name;
	input->input->name = name;
	input->input->id.bustype = 0;
	input->input->id.vendor  = 0x50fb;
	input->input->id.product = info->node;
	input->input->id.version = 0;
	return 0;
}

static void
l4fb_free_input_dev(struct l4x_event_source *source,
                    struct l4x_event_input_stream *input)
{
	kfree(input->input->name);
}


static struct l4x_event_input_source_ops l4_input_ops = {
	.setup_input = l4fb_setup_input_dev,
	.free_input  = l4fb_free_input_dev
};

static int l4fb_probe(struct platform_device *dev)
{
	struct fb_info *info;
	struct l4fb_screen *screen = container_of(dev, struct l4fb_screen, platform_device);
	int video_cmap_len;
	int ret = -ENOMEM;

	if (disable)
		return -ENODEV;

	/* Process module parameters */
	if (refreshsleep >= 0)
		screen->refresh_sleep = msecs_to_jiffies(refreshsleep);

	info = framebuffer_alloc(0, &dev->dev);
	if (!info)
		goto failed_after_screen_alloc;

	info->fbops = &l4fb_ops;
	info->var   = l4fb_defined;
	info->fix   = l4fb_fix;
	info->pseudo_palette = screen->pseudo_palette;
	info->flags = FBINFO_FLAG_DEFAULT;

	ret = l4fb_init_session(info, screen);
	if (ret) {
		if (verbose)
			l4x_printf("init error %d\n", ret);
		goto failed_after_framebuffer_alloc;
	}

	info->screen_base = (void *)info->fix.smem_start;
	if (!info->screen_base) {
		dev_err(&dev->dev,
		        "abort, graphic system could not be initialized.\n");
		ret = -EIO;
		goto failed_after_framebuffer_alloc;
	}

	/* some dummy values for timing to make fbset happy */
	info->var.pixclock    = 10000000 / info->var.xres
	                        * 1000 / info->var.yres;
	info->var.left_margin = (info->var.xres / 8) & 0xf8;
	info->var.hsync_len   = (info->var.xres / 8) & 0xf8;

	info->var.transp.length = 0;
	info->var.transp.offset = 0;

	video_cmap_len = 16;

	info->fix.ypanstep  = 0;
	info->fix.ywrapstep = 0;

	ret = fb_alloc_cmap(&info->cmap, video_cmap_len, 0);
	if (ret < 0)
		goto failed_after_framebuffer_alloc;

	dev_set_drvdata(&dev->dev, info);

	dev_info(&dev->dev, "%s L4 frame buffer device (refresh: %ujiffies)\n",
	         info->fix.id, screen->refresh_sleep);

	screen->input.event_cap = screen->goos;
	l4x_event_input_setup_source(&screen->input, INPUT_HASH_ENTRIES,
	                             &l4_input_ops);

	if (register_framebuffer(info) < 0) {
		ret = -EINVAL;
		goto failed_after_fb_alloc_cmap;
	}

	list_add(&screen->next_screen, &l4fb_screens);

	ret = device_create_file(&dev->dev, &dev_attr_refresh);
	if (ret)
		return ret;
	ret = device_create_file(&dev->dev, &dev_attr_refresh_sleep);
	if (ret)
		return ret;

	return 0;

failed_after_fb_alloc_cmap:
	fb_dealloc_cmap(&info->cmap);

failed_after_framebuffer_alloc:
	framebuffer_release(info);

failed_after_screen_alloc:
	l4fb_shutdown(screen);
	kfree(screen);

	return ret;
}

static int l4fb_alloc_screen(int id, char const *cap)
{
	struct l4fb_screen *screen;
	int ret;

	pr_info("l4fb.%d: looking for capability '%s' as goos session\n", id, cap);

	screen = kzalloc(sizeof(struct l4fb_screen), GFP_KERNEL);

	if (!screen)
		return -ENOMEM;

	l4fb_init_screen(screen);

	ret = L4XV_FN_i(l4x_re_resolve_name(cap, &screen->goos));

	if (ret) {
		pr_err("l4fb.%d: init failed err=%d\n", id, ret);
		kfree(screen);
		return ret;
	}

	screen->platform_device.id = id;
	screen->platform_device.name = "l4fb";
	ret = platform_device_register(&screen->platform_device);
	if (ret < 0) {
		dev_err(scr2dev(screen), "cannot register l4fb device (%d)\n", ret);
		kfree(screen);
		return ret;
	}
	/* l4fb events are wakeups per default */
	device_init_wakeup(&screen->platform_device.dev, true);
	return 0;
}

static int l4fb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);

	if (info) {
		struct l4fb_screen *screen = l4fb_screen(info);
		device_remove_file(&dev->dev, &dev_attr_refresh);
		device_remove_file(&dev->dev, &dev_attr_refresh_sleep);
		list_del(&screen->next_screen);
		unregister_framebuffer(info);
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);

		l4fb_shutdown(screen);
		kfree(screen);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int l4fb_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct l4fb_screen *screen = container_of(pdev, struct l4fb_screen,
	                                          platform_device);
	if (device_may_wakeup(dev))
		l4x_event_source_enable_wakeup(&screen->input, false);
	l4x_event_poll_source(&screen->input);
	return 0;
}

static int l4fb_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct l4fb_screen *screen = container_of(pdev, struct l4fb_screen,
	                                          platform_device);
	l4x_event_source_enable_wakeup(&screen->input, device_may_wakeup(dev));
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(l4fb_pm, l4fb_suspend, l4fb_resume);
static struct platform_driver l4fb_driver = {
	.probe   = l4fb_probe,
	.remove  = l4fb_remove,
	.driver  = {
		.name = "l4fb",
		.pm   = &l4fb_pm,
	},
};

static int __init l4fb_init(void)
{
	int ret;
	int i;

	l4fb_update_rect = l4fb_l4re_update_rect;

	ret = platform_driver_register(&l4fb_driver);
	if (ret)
		return ret;

	for (i = 0; i < num_fbs; ++i) {
		/* we ignore errors here, we just have no such device afterwards */
		if (l4fb_alloc_screen(i, fbs[i]))
			pr_warn("could not allocate fb device: %s\n", fbs[i]);
		else
			ret = 1;
	}

	/* we could not create any fb device so we are useless */
	if (!ret) {
		ret = -ENODEV;
		goto out;
	}

#ifdef CONFIG_L4_FB_DRIVER_DBG
	if (!IS_ERR(l4x_debugfs_dir))
		debugfs_dir = debugfs_create_dir("l4fb", NULL);
	if (!IS_ERR(debugfs_dir)) {
		debugfs_unmaps  = debugfs_create_u32("unmaps", S_IRUGO,
		                                     debugfs_dir,
		                                     &stats_unmaps);
		debugfs_updates = debugfs_create_u32("updates", S_IRUGO,
		                                     debugfs_dir,
		                                     &stats_updates);
	}
#endif
	return ret;
out:
	platform_driver_unregister(&l4fb_driver);
	return ret;
}
module_init(l4fb_init);

static void __exit l4fb_exit(void)
{
	struct l4fb_screen *screen, *tmp;
#ifdef CONFIG_L4_FB_DRIVER_DBG
	debugfs_remove(debugfs_unmaps);
	debugfs_remove(debugfs_updates);
	debugfs_remove(debugfs_dir);
#endif

	list_for_each_entry_safe(screen, tmp, &l4fb_screens, next_screen) {
		platform_device_unregister(&screen->platform_device);
	}
	platform_driver_unregister(&l4fb_driver);
}
module_exit(l4fb_exit);

MODULE_AUTHOR("Adam Lackorzynski <adam@os.inf.tu-dresden.de>");
MODULE_DESCRIPTION("Frame buffer driver for L4Re::Console's");
MODULE_LICENSE("GPL");


module_param(refreshsleep, int, 0600);
MODULE_PARM_DESC(refreshsleep, "Sleep between frame buffer refreshs in ms");
module_param(keeprefresh, int, 0600);
MODULE_PARM_DESC(keeprefresh, "Keep refresh also after update requests");
module_param(disable, int, 0);
MODULE_PARM_DESC(disable, "Disable driver");
module_param(touchscreen, int, 0);
MODULE_PARM_DESC(touchscreen, "Be a touchscreen");
module_param(verbose, int, 0600);
MODULE_PARM_DESC(verbose, "Tell more");
module_param_array(fbs, charp, &num_fbs, 0);
MODULE_PARM_DESC(fbs, "List of goos caps");
