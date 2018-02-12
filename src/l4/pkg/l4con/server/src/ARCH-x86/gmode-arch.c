/**
 * \file	con/server/src/ARCH-x86/gmode-arch.c
 * \brief	graphics mode initialization, x86 specific
 *
 * \date	2005
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>

#include <l4/sys/types.h>
#include <l4/sys/err.h>
#include <l4/x86emu/int10.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/util/video/goos_fb.h>
#include <l4/re/env.h>

#include "gmode.h"
#include "main.h"
#include "vc.h"
#include "con_hw/init.h"
#include "con_hw/iomem.h"

/** Try to detect and initialize the graphics card. */
static int
detect_hw(l4re_util_video_goos_fb_t *goosfb)
{
  unsigned char bipp;

  if (noaccel)
    return -L4_ENODEV;

  if (con_hw_init(fb_info.width, fb_info.height,
	          &bipp, fb_info.bytes_per_line,
		  (unsigned long)gr_vmem,
                  l4re_ds_size(l4re_util_video_goos_fb_buffer(goosfb)),
                  &hw_accel, &gr_vmem)<0)
    {
      /* fall back to VESA support */
      printf("No supported accelerated graphics card detected\n");
      return -L4_ENODEV;
    }

  fg_do_copy = hw_accel.copy;
  fg_do_fill = hw_accel.fill;
  fg_do_sync = hw_accel.sync;
  fg_do_drty = hw_accel.drty;

  if (hw_accel.caps & ACCEL_FAST_COPY)
    accel_caps |= L4CON_FAST_COPY;
  if (hw_accel.caps & ACCEL_FAST_CSCS_YV12)
    accel_caps |= L4CON_STREAM_CSCS_YV12;
  if (hw_accel.caps & ACCEL_FAST_CSCS_YUY2)
    accel_caps |= L4CON_STREAM_CSCS_YUY2;
  if (hw_accel.caps & ACCEL_POST_DIRTY)
    accel_caps |= L4CON_POST_DIRTY;
  if (hw_accel.caps & ACCEL_FAST_FILL)
    accel_caps |= L4CON_FAST_FILL;

  gr_vmem_maxmap = gr_vmem + YRES_CLIENT * fb_info.bytes_per_line;
  vis_vmem       = gr_vmem;
  vis_offs       = 0;
  if (gr_vmem_maxmap >= gr_vmem+gr_vmem_size)
    {
      printf("Framebuffer too small??\n");
      gr_vmem_maxmap = gr_vmem+gr_vmem_size;
    }

  printf("%s %d\n", __func__, __LINE__); 
  return 0;
}

/** Try to pan the memory */
static void
pan_vmem(void)
{
  l4_addr_t next_super_offs, start;
  int x, y, bpp;

  if (!pan)
    return;

  /* consider overflow on memory modes with more than 4MB */
  next_super_offs = l4_round_size(YRES_CLIENT * fb_info.bytes_per_line,
                                  L4_SUPERPAGESHIFT);

  if (next_super_offs + status_area * fb_info.bytes_per_line > gr_vmem_size)
    {
      printf("WARNING: Can't pan display: Only have %zdkB video memory "
	     "need %ldkB\n", gr_vmem_size >> 10, 
	     (next_super_offs + status_area*fb_info.bytes_per_line) >> 10);
      return;
    }

  bpp = (l4re_video_bits_per_pixel(&fb_info.pixel_info) + 1) / 8;
  x   = (next_super_offs % fb_info.bytes_per_line) / bpp;
  y   = (next_super_offs / fb_info.bytes_per_line) - YRES_CLIENT;

  /* pan graphics card */
  if (hw_accel.pan)
    {
      /* using hw-specific function */
      hw_accel.pan(&x, &y);
    }
  else
    {
      /* using VESA BIOS call */
      if (x86emu_int10_pan((unsigned*)&x, (unsigned*)&y) < 0)
	return;
    }

  pan_offs_x     = x;
  pan_offs_y     = y;
  start          = y*fb_info.bytes_per_line + x*bpp;

  gr_vmem_maxmap = gr_vmem + next_super_offs;
  vis_vmem       = gr_vmem + start;
  vis_offs       = start;

  printf("Display panned to %d:%d\n", y, x);
  panned = 1;
}

/** Initialize console driver */
void
init_gmode(void)
{
  /* Try to enable native hardware support for graphics card. */
  if (detect_hw(&goosfb) < 0)
    printf("HW driver not available\n");

  /* Try to force panning */
  pan_vmem();

  /* Release all memory occupied by int10 emulator */
  x86emu_int10_done();
}
