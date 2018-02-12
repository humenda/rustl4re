/*****************************************************************************/
/**
 * \file   input/lib/src/init.c
 * \brief  L4INPUT: Initialization
 *
 * \date   11/20/2003
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* L4 */
#include <l4/sys/err.h>
#include <l4/re/env.h>
#include <l4/util/kip.h>
#include <l4/io/io.h>
#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/rdtsc.h>  /* XXX x86 specific */
#endif

/* C */
#include <stdio.h>

/* local */
#include <l4/input/libinput.h>

#include "internal.h"

/** Backend operations */
static struct l4input_ops *ops;


int l4input_ispending()
{
	if (ops->ispending)
		return ops->ispending();
	else
		return 0;
}

L4_CV int l4input_flush(void *buf, int count)
{
	if (ops->flush)
		return ops->flush(buf, count);
	else
		return 0;
}

L4_CV int l4input_pcspkr(int tone)
{
	if (ops->pcspkr)
		return ops->pcspkr(tone);
	else
		return -L4_EINVAL;
}

/* Okay ...

   We have to initialize requested devices only here because there is only one
   general event device.

   After discussions with Norman I also think one event device is fine. Maybe
   we could add device ids to the event struct later and inject UPDATE events
   in the case of hotplugging.
*/

#define CHECK(x) if ((error=x)) { \
         printf("%s:%d: failed " #x ": %d\n", __FILE__, __LINE__, error); \
        return error; }

/** L4INPUT LIBRARY INITIALIZATION **/
L4_CV int l4input_init(int prio, L4_CV void (*handler)(struct l4input *))
{
	if (!l4util_kip_kernel_is_ux(l4re_kip())) {
		printf("L4INPUT native mode activated\n");

		/* for usleep */
#if defined(ARCH_x86) || defined(ARCH_amd64)
		l4_calibrate_tsc(l4re_kip());
#endif


		/* lib state */
		l4input_internal_irq_init(prio);
		l4input_internal_wait_init();

		printf("L4INPUT:                !!! W A R N I N G !!!\n"
		       "L4INPUT:  Please, do not use Fiasco's \"-esc\" with L4INPUT.\n"
		       "L4INPUT:                !!! W A R N I N G !!!\n");

		if (handler)
			printf("L4INPUT: Registered %p for callbacks.\n", handler);

		int error;
#if 0
#if defined(ARCH_x86) || defined(ARCH_amd64)
                CHECK(l4io_request_ioport(0x80, 1));
#endif
#endif
                CHECK(l4input_internal_input_init());
#if defined(ARCH_x86) || defined(ARCH_amd64)
		CHECK(l4input_internal_i8042_init());
#endif
		CHECK(l4input_internal_psmouse_init());
		CHECK(l4input_internal_atkbd_init());
#ifdef ARCH_arm
		l4input_internal_amba_kmi_init_k();
		l4input_internal_amba_kmi_init_m();

		l4input_internal_l4drv_init();
		l4input_internal_l4bus_init();
#endif
#if defined(ARCH_x86) || defined(ARCH_amd64)
                // reprogrammes PIT, only enable when you think you
                // know what you're doing
		//CHECK(l4input_internal_pcspkr_init());
#endif
		if (!(ops = l4input_internal_l4evdev_init(handler))) {
			printf("L4INPUT: evdev initialization failed\n");
			return -L4_ENODEV;
		}

	} else {
		printf("L4INPUT Fiasco-UX mode activated\n");

		l4input_internal_irq_init(prio);

#ifdef ARCH_x86
		if (!(ops = l4input_internal_ux_init(handler))) {
			printf("L4INPUT: Fiasco-UX H/W initialization failed\n");
			return -L4_ENODEV;
		}
#endif
	}

	return 0;
}
