#ifndef __ASM_L4__SERVER__INPUT_SRV_H__
#define __ASM_L4__SERVER__INPUT_SRV_H__

#include <asm/server/server.h>
#include <l4/re/event.h>

struct l4x_input_event
{
  unsigned long long time;       // struct timeval
  unsigned short     type;       // u16
  unsigned short     code;       // u16
    signed int       value;      // u32
  unsigned long      stream_id;
};

struct l4x_input_srv_ops
{
	L4_CV int (*num_streams)(void);
	L4_CV int (*stream_info)(int, l4re_event_stream_info_t *);
	L4_CV int (*stream_info_for_id)(l4_umword_t, l4re_event_stream_info_t *);
	L4_CV int (*axis_info)(l4_umword_t id, unsigned naxes, const unsigned *axis,
	                       l4re_event_absinfo_t *info);
};

C_FUNC void
l4x_srv_input_init(l4_cap_idx_t thread, struct l4x_input_srv_ops *ops);

C_FUNC void
l4x_srv_input_add_event(struct l4x_input_event *e);

C_FUNC void
l4x_srv_input_trigger(void);

#endif /* __ASM_L4__SERVER__INPUT_SRV_H__ */
