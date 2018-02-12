/*****************************************************************************/
/**
 * \file   input/lib/src/l4evdev.c
 * \brief  L4INPUT Event Device
 *
 * \date   05/28/2003
 * \author Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *
 * This is roughly a Linux /dev/event implementation adopted from evdev.
 *
 * Original copyright notice from drivers/input/evdev.c follows...
 */
/*
 * Event char devices, giving access to raw input device events.
 *
 * Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/* L4 */
#include <string.h>
#include <l4/sys/err.h>
#include <l4/input/libinput.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/env.h>
#include <linux/kernel.h>
#include <pthread.h>

/* C */
#include <stdio.h>

/* Linux */
#include <linux/input.h>
#include <linux/jiffies.h>

#include "internal.h"

/* Okay ...

   There can be up to L4EVDEV_DEVICES managed. 

   So we _don't_ need one event queue/list per device to support dedicated
   event handlers per device -> we use one general event queue (for DOpE) with
   up to L4EVDEV_BUFFER l4evdev_event structures. CON uses callbacks and
   therefore needs no ring buffer.
*/

#define L4EVDEV_DEVICES    16
#define L4EVDEV_BUFFER    512

#define DEBUG_L4EVDEV       0

/** l4evdev event structure */
struct l4evdev_event {
	struct l4evdev *evdev;
	struct l4input l4event;
};

/** l4evdev device structure */
struct l4evdev {
	int exists;
	int isopen;
	int devn;
	char name[16];
	struct input_handle handle;

	/* for touchpads */
	unsigned int pkt_count;
	int old_x[4], old_y[4];
	int frac_dx, frac_dy;
	unsigned long touch;
};

struct l4evdev *pcspkr;

/** all known devices */
static struct l4evdev l4evdev_devices[L4EVDEV_DEVICES];
/** global event list */
static struct l4evdev_event l4evdev_buffer[L4EVDEV_BUFFER];
static int l4evdev_buffer_head = 0;
static int l4evdev_buffer_tail = 0;
#define DEVS    l4evdev_devices
#define BUFFER  l4evdev_buffer
#define HEAD    l4evdev_buffer_head /**< next event slot */
#define TAIL    l4evdev_buffer_tail /**< first-in event if !(HEAD==TAIL) */
#define INC(x)  (x) = ((x) + 1) % L4EVDEV_BUFFER

static pthread_mutex_t l4evdev_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/*************************
 *** TOUCHPAD HANDLING ***
 *************************/
/* convert absolute to relative events for inappropriate devices like touchpads
 * (taken from input/mousedev.c) */
static unsigned int tpad_touch(struct l4evdev *mousedev, int value)
{
	if (!value) {
/* FIXME tap support */
#if 0
		if (mousedev->touch &&
		    time_before(jiffies, mousedev->touch + msecs_to_jiffies(tap_time))) {
			/*
			 * Toggle left button to emulate tap.
			 * We rely on the fact that mousedev_mix always has 0
			 * motion packet so we won't mess current position.
			 */
			set_bit(0, &mousedev->packet.buttons);
			set_bit(0, &mousedev_mix.packet.buttons);
			mousedev_notify_readers(mousedev, &mousedev_mix.packet);
			mousedev_notify_readers(&mousedev_mix, &mousedev_mix.packet);
			clear_bit(0, &mousedev->packet.buttons);
			clear_bit(0, &mousedev_mix.packet.buttons);
		}
#endif
		mousedev->touch = mousedev->pkt_count = 0;
		mousedev->frac_dx = 0;
		mousedev->frac_dy = 0;
	}
	else
		if (!mousedev->touch)
			mousedev->touch = jiffies;

	/* FIXME tap support */
	return KEY_RESERVED;
}

static unsigned int tpad_trans_key(unsigned int code)
{
	unsigned int button;

	switch (code) {

	/* not translated */
	case BTN_LEFT:
	case BTN_RIGHT:
	case BTN_MIDDLE:
	case BTN_SIDE:
	case BTN_EXTRA:
		button = code;
		break;

	/* translations */
	case BTN_TOUCH:
	case BTN_0:
		button = BTN_LEFT;
		break;
	case BTN_1:
		button = BTN_RIGHT;
		break;
	case BTN_2:
		button = BTN_MIDDLE;
		break;
	case BTN_3:
		button = BTN_SIDE;
		break;
	case BTN_4:
		button = BTN_EXTRA;
		break;

	/* abort per default */
	default:
		button = KEY_RESERVED;
	}

	return button;
}

#define fx(i)  (mousedev->old_x[(mousedev->pkt_count - (i)) & 03])
#define fy(i)  (mousedev->old_y[(mousedev->pkt_count - (i)) & 03])

/* check event and convert if appropriate
 * returns 1 if event should be ignored */
static int tpad_event(struct input_dev *dev, struct l4evdev *mousedev,
                      unsigned int *type0, unsigned int *code0, int *value0)
{
	int size, tmp;
	unsigned int type = *type0;
	unsigned int code = *code0;
	int value         = *value0;
	enum { FRACTION_DENOM = 128 };

	/* is it really a touchpad ? */
	if (!test_bit(BTN_TOOL_FINGER, dev->keybit))
		return 0;

	switch (type) {

	case EV_KEY:
		/* from input/mousedev.c:315 */

		/* ignore */
		if (value == 2) return 1;

		if (code == BTN_TOUCH)
			code = tpad_touch(mousedev, value);
		else
			code = tpad_trans_key(code);

		/* ignore some unhandled codes */
		if (code == KEY_RESERVED) return 1;
		break;


	case EV_SYN:
		/* from input/mousedev.c:324 */

		if (code == SYN_REPORT) {
			if (mousedev->touch) {
				mousedev->pkt_count++;
				/* Input system eats duplicate events, but we need all of them
				 * to do correct averaging so apply present one forward
				 */
				fx(0) = fx(1);
				fy(0) = fy(1);
			}
		}

		break;


	case EV_ABS:
		/* from input/mousedev.c:119 */

		/* ignore if pad was not touched */
		if (!mousedev->touch) return 1;

		size = dev->absmax[ABS_X] - dev->absmin[ABS_X];
		if (size == 0) size = 256 * 2;
		switch (code) {
		case ABS_X:
			fx(0) = value;
			/* ignore events with insufficient state */
			if (mousedev->pkt_count < 2) return 1;

			tmp = ((value - fx(2)) * (256 * FRACTION_DENOM)) / size;
			tmp += mousedev->frac_dx;
			value = tmp / FRACTION_DENOM;
			mousedev->frac_dx = tmp - value * FRACTION_DENOM;
			code = REL_X;
			type = EV_REL;
			break;

		case ABS_Y:
			fy(0) = value;
			/* ignore events with insufficient state */
			if (mousedev->pkt_count < 2) return 1;

			tmp = ((value - fy(2)) * (256 * FRACTION_DENOM)) / size;
			tmp += mousedev->frac_dy;
			value = tmp / FRACTION_DENOM;
			mousedev->frac_dy = tmp - value * FRACTION_DENOM;
			code = REL_Y;
			type = EV_REL;
			break;

		default: /* ignore this event */
			return 1;
		}
		break;

	default:
		/* ignore unknown events */
#if DEBUG_L4EVDEV
		printf("l4evdev.c: tpad ignored. D: %s, T: %d, C: %d, V: %d\n",
		       mousedev->handle.dev->phys, type, code, value);
#endif
		return 1;
	}

	/* propagate handled events */
	*type0 = type;
	*code0 = code;
	*value0 = value;
	return 0;
}

/** SOME EVENTS ARE FILTERED OUT **/
static inline int filter_event(struct input_handle *handle, unsigned int type,
                               unsigned int code, int value)
{
	/* filter sound driver events */
	if (test_bit(EV_SND, handle->dev->evbit))
          return 1;

	/* filter misc event: scancode -- no handlers yet */
	if ((type == EV_MSC) && (code == MSC_SCAN))
          return 1;

	/* accepted */
	return 0;
}


/** HANDLE INCOMING EVENTS (CALLBACK VARIANT) **/
static L4_CV void(*callback)(struct l4input *) = NULL;

static void l4evdev_event_cb(struct input_handle *handle, unsigned int type,
                             unsigned int code, int value)
{
#if DEBUG_L4EVDEV
	static unsigned long count = 0;
#endif
	static struct l4input ev;

	/* handle touchpads */
        if (0)
	if (tpad_event(handle->dev, (struct l4evdev *)handle->private,
	               &type, &code, &value))
		return;

	/* event filter */
	if (filter_event(handle, type, code, value)) return;

	ev.time = l4_kip_clock(l4re_kip());
	ev.type = type;
	ev.code = code;
	ev.value = value;
	ev.stream_id = (l4_umword_t)handle->dev;

	/* call back */
	callback(&ev);

#if DEBUG_L4EVDEV
	printf("l4evdev.c: Ev[%ld]. D: %s, T: %d, C: %d, V: %d\n",
	       count++, handle->dev->phys, type, code, value);
#endif
}

/** HANDLE INCOMING EVENTS (BUFFER AND PULL VARIANT) **/
static void l4evdev_event(struct input_handle *handle, unsigned int type,
                          unsigned int code, int value)
{
#if DEBUG_L4EVDEV
	static unsigned long count = 0;
#endif
	struct l4evdev *evdev = handle->private;
	l4_kernel_clock_t clk = l4_kip_clock(l4re_kip());

	/* handle touchpads */
        if (0)
	if (tpad_event(handle->dev, (struct l4evdev *)handle->private,
	               &type, &code, &value))
		return;

	/* event filter */
	if (filter_event(handle, type, code, value)) return;

        pthread_mutex_lock(&l4evdev_lock);

	BUFFER[HEAD].evdev = evdev;
	BUFFER[HEAD].l4event.time = clk;
	BUFFER[HEAD].l4event.type = type;
	BUFFER[HEAD].l4event.code = code;
	BUFFER[HEAD].l4event.value = value;
	BUFFER[HEAD].l4event.stream_id = (l4_umword_t)handle->dev;

	INC(HEAD);

	/* check head and tail */
	if (HEAD == TAIL) {
		/* clear oldest event struct in buffer */
		//memset(&BUFFER[TAIL], 0, sizeof(struct l4evdev_event));
		INC(TAIL);
	}

	pthread_mutex_unlock(&l4evdev_lock);

#if DEBUG_L4EVDEV
	printf("l4evdev.c: Ev[%ld]. D: %s, T: %d, C: %d, V: %d\n",
	       count++, handle->dev->phys, type, code, value);
#endif
}

struct l4evdev *get_next_evdev(int *devnp)
{
        int devn;

	for (devn = 0; (devn < L4EVDEV_DEVICES) && (DEVS[devn].exists); devn++)
          ;

	if (devn == L4EVDEV_DEVICES) {
		printf("l4evdev.c: no more free l4evdev devices\n");
		return NULL;
	}

        *devnp = devn;

        return &DEVS[devn];
}

/* XXX had connect/disconnect to be locked? */

static struct input_handle * l4evdev_connect(struct input_handler *handler,
                                             struct input_dev *dev,
                                             struct input_device_id *id)
{
	int devn;
	struct l4evdev *evdev = get_next_evdev(&devn);

        if (!evdev)
                return NULL;

	memset(evdev, 0, sizeof (struct l4evdev));

	evdev->exists = 1;
	evdev->devn = devn;

	sprintf(evdev->name, "event%d", devn);

	evdev->handle.dev = dev;
	evdev->handle.name = evdev->name;
	evdev->handle.handler = handler;
	evdev->handle.private = evdev;

	input_open_device(&evdev->handle);

	printf("connect \"%s\", %s\n", dev->name, dev->phys);

	if (test_bit(EV_SND, dev->evbit))
		pcspkr = evdev;

	return &evdev->handle;
}

static void l4evdev_disconnect(struct input_handle *handle)
{
	struct l4evdev *evdev = handle->private;

	evdev->exists = 0;

	if (evdev->isopen) {
		input_close_device(handle);
		if (test_bit(EV_SND, handle->dev->evbit))
			pcspkr = NULL;
        } else /* XXX what about pending events? */
		memset(&DEVS[evdev->devn], 0, sizeof(struct l4evdev));
}

static struct input_device_id l4evdev_ids[] = {
	{ .driver_info = 1 },  /* Matches all devices */
	{ },                   /* Terminating zero entry */
};

static struct input_handler l4evdev_handler = {
	.event =      NULL,               /* fill it on init() */
	.connect =    l4evdev_connect,
	.disconnect = l4evdev_disconnect,
	.fops =       NULL,               /* not used */
	.minor =      0,                  /* not used */
	.name =       "l4evdev",
	.id_table =   l4evdev_ids
};

/*****************************************************************************/

static int l4evdev_ispending(void)
{
	return !(HEAD==TAIL);
}

static int l4evdev_flush(void *buf, int count)
{
	int num = 0;
	struct l4input *buffer = buf;

	pthread_mutex_lock(&l4evdev_lock);

	while ((TAIL!=HEAD) && count) {
		/* flush event buffer */
                *buffer = BUFFER[TAIL].l4event;

		//memset(&BUFFER[TAIL], 0, sizeof(struct l4evdev_event));

		num++; count--;
		INC(TAIL);
		buffer ++;
	}

	pthread_mutex_unlock(&l4evdev_lock);

	return num;
}

static void l4_input_fill_info(struct input_dev *dev, l4re_event_stream_info_t *info)
{
	memset(info, 0, sizeof(*info));
	info->stream_id = (l4_umword_t)dev;

	info->id.bustype = dev->id.bustype;
	info->id.vendor  = dev->id.vendor;
	info->id.product = dev->id.product;
	info->id.version = dev->id.version;
#define COPY_BITS(n) memcpy(info->n ##s, dev->n, min(sizeof(info->n ##s), sizeof(dev->n)))

	COPY_BITS(evbit);
	COPY_BITS(keybit);
	COPY_BITS(relbit);
	COPY_BITS(absbit);
#undef COPY_BITS
}


L4_CV int
l4evdev_stream_info_for_id(l4_umword_t id, l4re_event_stream_info_t *si)
{
	unsigned devn;
	for (devn = 0; devn < L4EVDEV_DEVICES; devn++) {
		if (!DEVS[devn].exists)
			continue;
		if ((l4_umword_t)DEVS[devn].handle.dev == id) {
			l4_input_fill_info(DEVS[devn].handle.dev, si);
			return 0;
		}
	}

	return -L4_EINVAL;
}

L4_CV int
l4evdev_absinfo(l4_umword_t id, unsigned naxes, unsigned const *axes,
                l4re_event_absinfo_t *infos)
{
	unsigned devn, idx;
	struct input_dev *dev;
	for (devn = 0; devn < L4EVDEV_DEVICES; devn++) {
		if (!DEVS[devn].exists)
			continue;
		if ((l4_umword_t)DEVS[devn].handle.dev == id) {
			break;
		}
	}

	if (devn == L4EVDEV_DEVICES)
		return -L4_EINVAL;

	dev = DEVS[devn].handle.dev;
	for (idx = 0; idx < naxes; idx++) {
		unsigned a = axes[idx];
		if (a > ABS_MAX)
			return -L4_EINVAL;

		infos[idx].value = 0;
		infos[idx].min = dev->absmin[a];
		infos[idx].max = dev->absmax[a];
		infos[idx].fuzz = dev->absfuzz[a];
		infos[idx].flat = dev->absflat[a];
		infos[idx].resolution = 0;
	}
	return 0;
}

static int l4evdev_pcspkr(int tone)
{
	if (!pcspkr)
		return -L4_ENODEV;

	input_event(pcspkr->handle.dev, EV_SND, SND_TONE, tone);

	return 0;
}

static struct l4input_ops ops = {
	l4evdev_ispending, l4evdev_flush, l4evdev_pcspkr
};

struct l4input_ops * l4input_internal_l4evdev_init(L4_CV void (*cb)(struct l4input *))
{
	if (cb) {
		/* do callbacks */
		callback = cb;
		l4evdev_handler.event = l4evdev_event_cb;
	} else {
		/* buffer events in ring buffer */
		l4evdev_handler.event = l4evdev_event;
	}

	input_register_handler(&l4evdev_handler);

	return &ops;
}

void l4input_internal_register_ux(struct input_dev *dev)
{
        int devn;
	struct l4evdev *evdev = get_next_evdev(&devn);

        if (!evdev)
                return;

        printf("EVDEV-NR: %d\n", devn);

        evdev->exists = 1;
        evdev->devn = devn;

        sprintf(evdev->name, "event%d", devn);

        evdev->handle.dev = dev;
        evdev->handle.name = evdev->name;
        evdev->handle.handler = 0;
        evdev->handle.private = evdev;
}

void l4input_internal_l4evdev_exit(void)
{
	input_unregister_handler(&l4evdev_handler);
}
