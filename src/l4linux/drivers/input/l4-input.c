#include <asm/generic/event-input.h>

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>

static void init_axis(struct l4x_event_source *source, l4_umword_t id,
                      struct input_dev *dev, unsigned axis, int verbose)
{
	unsigned _axis = axis;
	l4re_event_absinfo_t ainfo;
	int i = L4XV_FN_i(l4re_event_get_axis_info(source->event_cap, id, 1,
	                                           &_axis, &ainfo));
	if (i < 0) {
		dev_err(&dev->dev, "could not get axis info for device %lx, "
		        "axis %d: result %d\n", id, axis, i);
		clear_bit(axis, dev->absbit);
	} else {
		if (verbose)
			dev_info(&dev->dev, "axis info[%lx,%d]: min=%d,"
			         "max=%d, fuzz=%d, flat=%d\n",
			         id, axis, ainfo.min,
			         ainfo.max, ainfo.fuzz, ainfo.flat);
		input_set_abs_params(dev, axis, ainfo.min, ainfo.max,
		                     ainfo.fuzz, ainfo.flat);
	}
}

int l4x_event_input_setup_stream_infos(struct l4x_event_source *source,
                                       struct l4x_event_input_stream *input)
{
	int err;
	l4re_event_stream_info_t sinfo;
	struct input_dev *dev = input->input;

	err = L4XV_FN_i(l4re_event_get_stream_info_for_id(source->event_cap,
	                                                  input->stream.id,
	                                                  &sinfo));
	if (err < 0) {
		dev_err(&dev->dev, "could not get infos for input "
		       "device %lx: error = %d\n", input->stream.id, err);
		return err;
	}

	bitmap_copy(dev->evbit, sinfo.evbits,
	            min(EV_MAX, L4RE_EVENT_EV_MAX) + 1);
#define COPY_BM(N, n) \
	if (test_bit(EV_##N, dev->evbit)) \
		bitmap_copy(dev->n##bit, sinfo.n##bits, \
		            min(N##_MAX, L4RE_EVENT_##N##_MAX) + 1)

	COPY_BM(KEY, key);
	COPY_BM(ABS, abs);
	COPY_BM(REL, rel);
#undef COPY_BM

	if (input->simulate_touchscreen && test_bit(BTN_MOUSE, dev->keybit))
		set_bit(BTN_TOUCH, dev->keybit);

	bitmap_copy(dev->propbit, sinfo.propbits,
	            min(INPUT_PROP_MAX, L4RE_EVENT_PROP_MAX) + 1);

	if (test_bit(EV_ABS, dev->evbit)) {
		unsigned a;
		for (a = 0; a <= ABS_MAX; ++a)
			if (test_bit(a, dev->absbit))
				init_axis(source, input->stream.id, dev, a, 0);
	}

	return 0;
}
EXPORT_SYMBOL(l4x_event_input_setup_stream_infos);

static struct l4x_event_stream *
l4x_new_input_device(struct l4x_event_source *source, unsigned long id)
{
	int err;
	struct l4x_event_input_stream *stream;
	struct input_dev *dev;
	struct l4x_event_input_source_ops *ops;
	
	ops = container_of(source->ops, struct l4x_event_input_source_ops,
	                   source);

	stream = kzalloc(sizeof(struct l4x_event_input_stream), GFP_KERNEL);
	if (!stream)
		return NULL;

	dev = input_allocate_device();
	if (!dev)
		goto out_free_stream;

	stream->stream.id = id;
	stream->input = dev;

	if (ops->setup_input(source, stream) < 0)
		goto out_free_input_dev;

	err = input_register_device(dev);
	if (err) {
		dev_err(&dev->dev, "could not register input device: %d\n",
		        err);
		goto out_free_input_dev;
	}

	msleep(500); /* Opps, but injecting too fast can loose events */
	return &stream->stream;

out_free_input_dev:
	input_free_device(dev);

out_free_stream:
	kfree(stream);
	return NULL;
}

static void l4x_event_input_free(struct l4x_event_source *source,
                                 struct l4x_event_stream *stream)
{
	struct l4x_event_input_stream *input;
	struct l4x_event_input_source_ops *ops;

	input = container_of(stream, struct l4x_event_input_stream, stream);
	ops = container_of(source->ops, struct l4x_event_input_source_ops,
	                   source);

	if (ops->free_input)
		ops->free_input(source, input);
	
	if (input->input) {
		input_unregister_device(input->input);
		input_free_device(input->input);
		input->input = 0;
	}

	kfree(input);
}

static void l4x_event_input_process(struct l4x_event_stream *stream,
                                    l4re_event_t const *event)
{
	struct l4x_event_input_stream *input;
	int code = event->code;

	if (system_state != SYSTEM_RUNNING)
		return;

	input = container_of(stream, struct l4x_event_input_stream, stream);

	if (input->simulate_touchscreen
	    && event->type == EV_KEY && code == BTN_LEFT)
		code = BTN_TOUCH;

	if (input->input)
		input_event(input->input, event->type, code, event->value);
}

int l4x_event_input_setup_source(struct l4x_event_source *source,
                                 unsigned long hash_entries,
                                 struct l4x_event_input_source_ops *ops)
{
	ops->source.process = l4x_event_input_process;
	ops->source.new_stream = l4x_new_input_device;
	ops->source.free_stream = l4x_event_input_free;
	return l4x_event_setup_source(source, hash_entries, &ops->source);
}
EXPORT_SYMBOL(l4x_event_input_setup_source);
