/*!
 * \file	init.c
 * \brief	init hardware accel stuff
 *
 * \date	07/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2002-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <l4/sys/err.h>
#include <l4/util/rdtsc.h>
#include <l4/sys/kdebug.h>
#include <l4/re/env.h>
#include <l4/util/kip.h>

#include "init.h"
#include "pci.h"
#include "ati.h"
#include "ati128.h"
#include "radeon.h"
#include "intel.h"
#include "matrox.h"
#include "riva.h"
#include "savage.h"
#include "vmware.h"
#include "ux.h"

l4_addr_t      hw_vid_mem_addr, hw_vid_mem_size;
l4_addr_t      hw_map_vid_mem_addr;
unsigned short hw_xres, hw_yres;
unsigned char  hw_bits;
unsigned short hw_bpl;

static int init_done = 0;

int
con_hw_init(unsigned short xres, unsigned short yres, unsigned char *bits,
            unsigned short bpl,
	    l4_addr_t vid_mem_addr, l4_size_t vid_mem_size,
	    con_accel_t *accel, l4_uint8_t **map_vid_mem_addr)
{
  if (init_done)
    return -L4_EINVAL;

  l4_calibrate_tsc(l4re_kip());

  hw_vid_mem_addr = vid_mem_addr;
  hw_vid_mem_size = vid_mem_size;
  hw_xres = xres;
  hw_yres = yres;
  hw_bits = *bits;
  hw_bpl  = bpl;

#ifdef ARCH_x86
  if (l4util_kip_kernel_is_ux(l4re_kip()))
    {
      printf("Tuning for Fiasco-UX\n");

      if (ux_probe(accel))
        return -L4_ENODEV;

      *map_vid_mem_addr = (void*)hw_map_vid_mem_addr;
      *bits = hw_bits;

      return 0;
    }
#endif

  radeon_register();
  ati128_register();
  ati_register();
  intel_register();
  matrox_register();
  riva_register();
  savage_register();
  vmware_register();

  if (pci_probe(accel)==0)
    {
      /* set by probe functions */
      printf("Backend scaler: %s, color keying: %s\n",
    	  (accel->caps & (ACCEL_FAST_CSCS_YV12|ACCEL_FAST_CSCS_YUY2)) 
	    ? (accel->caps & ACCEL_FAST_CSCS_YV12) ? "YV12" : "YUY2"
	    : "no",
	  (accel->caps & ACCEL_COLOR_KEY) ? "yes" : "no");
      *map_vid_mem_addr = (void*)hw_map_vid_mem_addr;
      *bits = hw_bits;
//      accel->caps &= ~(ACCEL_FAST_CSCS_YV12|ACCEL_FAST_CSCS_YUY2);
      return 0;
    }

  return -L4_ENODEV;
}
