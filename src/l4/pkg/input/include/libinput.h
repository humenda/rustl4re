/*****************************************************************************/
/**
 * \file   input/include/libinput.h
 * \brief  Input event library (L4INPUT) API
 *
 * \date   11/20/2003
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 */
/* (c) 2003 Technische Universitaet Dresden
 * This file is part of DROPS, which is distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * Original copyright notice from include/linux/input.h follows...
 */
/*
 * Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef __INPUT_INCLUDE_LIBINPUT_H_
#define __INPUT_INCLUDE_LIBINPUT_H_

#include <l4/sys/compiler.h>
#include <l4/input/macros.h>
#include <l4/re/event.h>

EXTERN_C_BEGIN

struct l4input {
        long long time; ///< unused on bare hardware, used in Fiasco-UX
	unsigned short type;
	unsigned short code;
	int value;
	unsigned long stream_id;
};

/** Initialize input driver library.
 *
 * \param prio      if != L4THREAD_DEFAULT_PRIO use as prio for irq threads
 * \param handler   if !NULL use this function on event occurence as
 *                  callback
 *
 * libinput works in 2 different modes:
 *
 * -# input events are stored in library local ring buffers until
 * l4input_flush() is called.
 *
 * -# on each event occurrence \a handler is called and no local event
 * buffering is done.
 */
L4_CV int l4input_init(int prio, L4_CV void (*handler)(struct l4input *));

/** Query event status.
 *
 * \return 0 if there are no pending events; !0 otherwise
 */
L4_CV int l4input_ispending(void);

/** Get events.
 *
 * \param buffer    event return buffer
 * \param count     max number of events to return
 *
 * \return number of flushed events
 *
 * Returns up to \a count events into buffer.
 */
L4_CV int l4input_flush(void *buffer, int count);

/** Program PC speaker
 *
 * \param tone      tone value (0 switches off)
 */
L4_CV int l4input_pcspkr(int tone);

L4_CV int l4evdev_stream_info_for_id(l4_umword_t id, l4re_event_stream_info_t *si);
L4_CV int l4evdev_absinfo(l4_umword_t id, unsigned naxes, const unsigned *axes, l4re_event_absinfo_t *infos);

EXTERN_C_END

#endif

