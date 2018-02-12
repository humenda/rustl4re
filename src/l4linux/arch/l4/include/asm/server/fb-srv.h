/*
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See file COPYING-GPL-2 for details.
 */
#ifndef __ASM_L4__SERVER__FB_SRV_H__
#define __ASM_L4__SERVER__FB_SRV_H__

#include <l4/re/c/video/goos.h>

#include <asm/server/server.h>

struct l4fb_info
{
	l4re_video_goos_info_t i;
	l4_uint32_t            bytes_per_scanline;
	l4_addr_t              screen_base, screen_size;
};

C_FUNC void
l4x_fb_srv_set(struct l4fb_info *info, l4_cap_idx_t thread);

#endif /* __ASM_L4__SERVER__FB_SRV_H__ */
