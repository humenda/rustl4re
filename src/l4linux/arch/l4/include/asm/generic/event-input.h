#ifndef __ASM_L4__GENERIC__L4_EVENT_INPUT_H__
#define __ASM_L4__GENERIC__L4_EVENT_INPUT_H__

#include <linux/kernel.h>

#include <asm/generic/event.h>

enum { L4X_EVENT_INPUT_MAX_DEV_NAME = 20 };

struct input_dev;

struct l4x_event_input_stream {
	struct l4x_event_stream stream;
	struct input_dev *input;
	int simulate_touchscreen;
	char name[L4X_EVENT_INPUT_MAX_DEV_NAME];
};

struct l4x_event_input_source_ops {
	struct l4x_event_source_ops source;
	int (*setup_input)(struct l4x_event_source *source,
	                   struct l4x_event_input_stream *stream);
	void (*free_input)(struct l4x_event_source *source,
	                   struct l4x_event_input_stream *stream);
};

int l4x_event_input_setup_source(struct l4x_event_source *source,
                                 unsigned long hash_entries,
                                 struct l4x_event_input_source_ops *ops);

int l4x_event_input_setup_stream_infos(struct l4x_event_source *source,
                                       struct l4x_event_input_stream *input);

#endif /* __ASM_L4__GENERIC__L4_EVENT_INPUT_H__ */
