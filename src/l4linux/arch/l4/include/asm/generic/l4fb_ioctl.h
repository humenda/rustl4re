#ifndef __INCLUDE__ASM__GENERIC__L4FB_IOCTL_H__
#define __INCLUDE__ASM__GENERIC__L4FB_IOCTL_H__

#include <linux/ioctl.h>

struct l4fb_region {
	int x;
	int y;
	int w;
	int h;
};

struct l4fb_set_viewport {
	int view;
	struct l4fb_region r;
};

struct l4fb_stack_view {
	int view;
	int pivot;
	int behind;
};

struct l4fb_view_flags {
	int view;
	unsigned long flags;
};

enum l4fb_ioctl_enums {
	L4FB_IOCTL_CREATE_VIEW    = _IOW('F', 0x50, int),
	L4FB_IOCTL_DESTROY_VIEW   = _IOW('F', 0x51, int),
	L4FB_IOCTL_BACK_VIEW      = _IOW('F', 0x52, int),
	L4FB_IOCTL_PLACE_VIEW     = _IOW('F', 0x53, struct l4fb_set_viewport),
	L4FB_IOCTL_STACK_VIEW     = _IOW('F', 0x54, struct l4fb_stack_view),
	L4FB_IOCTL_REFRESH        = _IOW('F', 0x55, struct l4fb_region),
	L4FB_IOCTL_VIEW_SET_FLAGS = _IOW('F', 0x56, struct l4fb_view_flags)
};

#endif /* ! __INCLUDE__ASM__GENERIC__L4FB_IOCTL_H__ */
