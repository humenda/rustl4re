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
#include <stdlib.h>

#include <l4/sys/l4int.h>
#include <l4/drivers/lcd.h>

#include "gmode.h"
#include "vc.h"

void
init_gmode(void)
{
  struct arm_lcd_ops *lcd;

  if (!(lcd = arm_lcd_probe(NULL)))
    {
      printf("Could not find LCD.\n");
      return;
    }

  printf("Using LCD driver: %s\n", lcd->get_info());

  if (lcd->get_fbinfo(&fb_info))
    {
      printf("Could not get framebuffer info\n");
      return;
    }

  YRES_CLIENT           = fb_info.height - status_area;
  fb_info.buffer_offset = 0;

  gr_vmem         = lcd->get_fb();
  gr_vmem_size    = lcd->get_video_mem_size();
  gr_vmem_maxmap  = gr_vmem + gr_vmem_size;
  vis_vmem        = gr_vmem;
  if (!gr_vmem)
    {
      printf("Could not setup video memory\n");
      exit(1);
    }

  lcd->enable();
}
