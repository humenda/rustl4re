#ifndef __ASM_L4__GENERIC__L4_EVENT_H__
#define __ASM_L4__GENERIC__L4_EVENT_H__

#include <linux/list.h>

#include <asm/generic/l4lib.h>

#include <l4/sys/types.h>
#include <l4/re/c/event_buffer.h>

struct l4x_event_source;

struct l4x_event_stream {
	struct hlist_node link;
	unsigned long id;
};

struct l4x_event_source_ops {
	void (*process)(struct l4x_event_stream *stream,
	                l4re_event_t const *event);
	struct l4x_event_stream *(*new_stream)(struct l4x_event_source *src,
	                                       unsigned long id);
        void (*free_stream)(struct l4x_event_source *src,
                            struct l4x_event_stream *stream);
};


struct l4x_event_source {
	struct l4x_event_source_ops *ops;
	l4_cap_idx_t event_cap;
	l4_cap_idx_t data_space;
	l4_cap_idx_t irq;
	long irq_num;
	l4re_event_buffer_consumer_t events;
	unsigned long hash_mask;
	struct hlist_head *streams;
};


int l4x_event_setup_source(struct l4x_event_source *source,
                          unsigned long hash_entries,
                          struct l4x_event_source_ops *ops);

void l4x_event_init_source(struct l4x_event_source *source);

void l4x_event_shutdown_source(struct l4x_event_source *source);

int l4x_event_source_register_stream(struct l4x_event_source *source,
                                     struct l4x_event_stream *stream);

int l4x_event_source_unregister_stream(struct l4x_event_source *source,
                                       struct l4x_event_stream *stream);
void l4x_event_poll_source(struct l4x_event_source *source);

int l4x_event_source_enable_wakeup(struct l4x_event_source *source,
                                   bool enable);
#endif
