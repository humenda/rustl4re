#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <asm/generic/cap_alloc.h>
#include <asm/generic/l4lib.h>
#include <asm/generic/event.h>

#include <l4/sys/icu.h>
#include <l4/sys/err.h>
#include <l4/sys/task.h>
#include <l4/sys/factory.h>

#include <l4/re/env.h>
#include <l4/re/event_enums.h>
#include <l4/re/c/event_buffer.h>

L4_EXTERNAL_FUNC(l4re_event_buffer_consumer_foreach_available_event);
L4_EXTERNAL_FUNC(l4re_event_get_buffer);
L4_EXTERNAL_FUNC(l4re_event_buffer_attach);
L4_EXTERNAL_FUNC(l4re_event_buffer_detach);
L4_EXTERNAL_FUNC(l4re_event_get_stream_info_for_id);
L4_EXTERNAL_FUNC(l4re_event_get_axis_info);

L4_CV static void l4x_event_put(l4re_event_t *event, void *data)
{
	struct l4x_event_source *source = (struct l4x_event_source *)data;
	struct l4x_event_stream *stream = 0;
	struct hlist_head *bucket;

	bucket = &source->streams[event->stream_id & source->hash_mask];

	hlist_for_each_entry(stream, bucket, link) {
		if (stream->id == event->stream_id)
			break;
	}

	if (unlikely(!stream)) {
		if (unlikely(event->type == L4RE_EV_SYN &&
		             event->code == L4RE_SYN_STREAM_CFG) &&
		    event->value == L4RE_SYN_STREAM_CLOSE)
			return;

		if (!source->ops->new_stream)
			return;

		stream = source->ops->new_stream(source, event->stream_id);
		if (unlikely(!stream))
			return;

		stream->id = event->stream_id;
		hlist_add_head(&stream->link, bucket);
	}

	if (unlikely(event->type == L4RE_EV_SYN &&
	             event->code == L4RE_SYN_STREAM_CFG) &&
	    event->value == L4RE_SYN_STREAM_CLOSE) {
		hlist_del(&stream->link);
		if (source->ops->free_stream)
			source->ops->free_stream(source, stream);
		return;
	} else
		source->ops->process(stream, event);
}

static irqreturn_t l4x_event_interrupt_th(int irq, void *data)
{
	struct l4x_event_source *source = (struct l4x_event_source *)data;
	l4re_event_buffer_consumer_foreach_available_event(&source->events,
	                                                   data,
	                                                   l4x_event_put);
	return IRQ_HANDLED;
}

void l4x_event_poll_source(struct l4x_event_source *source)
{
	l4re_event_buffer_consumer_foreach_available_event(&source->events,
	                                                   source,
	                                                   l4x_event_put);
}
EXPORT_SYMBOL(l4x_event_poll_source);

static irqreturn_t l4x_event_interrupt_ll(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

void l4x_event_init_source(struct l4x_event_source *source)
{
	source->irq_num = -1;
	source->data_space = L4_INVALID_CAP;
	source->irq = L4_INVALID_CAP;
}
EXPORT_SYMBOL(l4x_event_init_source);

int l4x_event_source_enable_wakeup(struct l4x_event_source *source,
                                   bool enable)
{
	if (enable) {
		enable_irq_wake(source->irq_num);
		disable_irq(source->irq_num);
	} else {
		enable_irq(source->irq_num);
		disable_irq_wake(source->irq_num);
	}
	return 0;
}
EXPORT_SYMBOL(l4x_event_source_enable_wakeup);

int l4x_event_setup_source(struct l4x_event_source *source,
                          unsigned long hash_entries,
                          struct l4x_event_source_ops *ops)
{
	int res = -ENOMEM;

	source->irq_num = -1;
	source->data_space = L4_INVALID_CAP;
	source->irq = L4_INVALID_CAP;

	source->streams = kzalloc(sizeof(struct hlist_head) * hash_entries,
	                          GFP_KERNEL);
	if (!source->streams)
		return -ENOMEM;

	source->hash_mask = hash_entries - 1;
	source->ops = ops;

	if (l4_is_invalid_cap(source->data_space = l4x_cap_alloc()))
		return res;

	if (L4XV_FN_i(l4re_event_get_buffer(source->event_cap,
	                                    source->data_space))) {
		res = -ENODEV;
		goto out_free_ds_cap;
	}

	res = -ENOENT;
	if (L4XV_FN_i(l4re_event_buffer_attach(&source->events,
	                                       source->data_space,
	                                       l4re_env()->rm)))
		goto out_release_ds;

	res = -ENOMEM;
	if (l4_is_invalid_cap(source->irq = l4x_cap_alloc()))
		goto out_detach_event;

	if (L4XV_FN_e(l4_factory_create_irq(l4re_env()->factory, source->irq)))
		goto out_free_irq_cap;

	res = -ENOENT;
	if (L4XV_FN_e(l4_icu_bind(source->event_cap, 0, source->irq)))
		goto out_release_irq;

	res = -ENOMEM;
	if ((source->irq_num = l4x_register_irq(source->irq)) < 0)
		goto out_release_irq;

	res = request_threaded_irq(source->irq_num, l4x_event_interrupt_ll,
	                           l4x_event_interrupt_th,
	                           IRQF_TRIGGER_RISING, "l4event", source);
	if (res < 0)
		goto out_unregister_irq;

	return 0;

out_unregister_irq:
	l4x_unregister_irq(source->irq_num);
	source->irq_num = -1;

out_release_irq:
	L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP, source->irq));

out_free_irq_cap:
	l4x_cap_free(source->irq);
	source->irq = L4_INVALID_CAP;

out_detach_event:
	L4XV_FN_v(l4re_event_buffer_detach(&source->events, l4re_env()->rm));

out_release_ds:
	L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP, source->data_space));

out_free_ds_cap:
	l4x_cap_free(source->data_space);
	source->data_space = L4_INVALID_CAP;
	return res;
}
EXPORT_SYMBOL(l4x_event_setup_source);

void l4x_event_shutdown_source(struct l4x_event_source *source)
{
	unsigned long i;

	if (source->irq_num >= 0) {
		L4XV_FN_v(l4_icu_unbind(source->event_cap, 0, source->irq));
		free_irq(source->irq_num, NULL);
		l4x_unregister_irq(source->irq_num);
		source->irq_num = -1;
	}

	if (!l4_is_invalid_cap(source->irq)) {
		L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP,
		                              source->irq));
		l4x_cap_free(source->irq);
		source->irq = L4_INVALID_CAP;
	}

	L4XV_FN_v(l4re_event_buffer_detach(&source->events, l4re_env()->rm));

	if (!l4_is_invalid_cap(source->data_space)) {
		L4XV_FN_v(l4_task_release_cap(L4RE_THIS_TASK_CAP,
		                              source->data_space));
		l4x_cap_free(source->data_space);
		source->data_space = L4_INVALID_CAP;
	}

	if (!source->streams)
		return;

	for (i = 0; i <= source->hash_mask; ++i) {
		struct l4x_event_stream *stream;
		struct hlist_node *n;
		hlist_for_each_entry_safe(stream, n,
		                          &source->streams[i], link) {
			if (source->ops->free_stream)
				source->ops->free_stream(source, stream);
		}
	}

	kfree(source->streams);
	source->streams = NULL;
	source->hash_mask = 0;

}
EXPORT_SYMBOL(l4x_event_shutdown_source);

int l4x_event_source_register_stream(struct l4x_event_source *source,
                                     struct l4x_event_stream *stream)
{
	struct hlist_head *bucket;
	bucket = &source->streams[stream->id & source->hash_mask];
	hlist_add_head(&stream->link, bucket);
	return 0;
}
EXPORT_SYMBOL(l4x_event_source_register_stream);

int l4x_event_source_unregister_stream(struct l4x_event_source *source,
                                       struct l4x_event_stream *stream)
{
	hlist_del(&stream->link);
	return 0;
}
EXPORT_SYMBOL(l4x_event_source_unregister_stream);
