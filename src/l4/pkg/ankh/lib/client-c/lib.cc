#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/cxx/ipc_stream>
#include <l4/ankh/protocol>
#include <l4/ankh/shm>
#include <l4/ankh/session>
#include <l4/shmc/ringbuf.h>
#include <l4/ankh/client-c.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/debugger.h>
#include <cstdio>

// XXX: support several ANKH connections
static L4::Cap<void> ankh_server;
l4shmc_area_t ankh_shmarea;
l4shmc_ringbuf_t _snd, _rcv;
l4shmc_chunk_t ankh_info_chunk;
AnkhSessionDescriptor* ankh_conn_info = NULL;
int _initialized = 0;
int _snd_init = 0;
int _rcv_init = 0;

L4_CV l4shmc_ringbuf_t *l4ankh_get_sendbuf(void) L4_NOTHROW
{	return &_snd; }

L4_CV l4shmc_ringbuf_t *l4ankh_get_recvbuf(void) L4_NOTHROW
{	return &_rcv; }

L4_CV int l4ankh_init(void) L4_NOTHROW
{
	if (!(ankh_server = L4Re::Env::env()->get_cap<void>("ankh"))) {
		printf("Could not find Ankh server.\n");
		return 1;
	}

	memset(&_snd, 0, sizeof(_snd));
	memset(&_rcv, 0, sizeof(_rcv));

	return 0;
}


/*
 * XXX:
 *	* pass a cap name as parameter to support multiple connections
 *	* return a local handle (or a struct ptr)
 */
L4_CV int
l4ankh_open(char *shm_name, int bufsize) L4_NOTHROW
{
	if (!_initialized)
		if (l4ankh_init())
			return -L4_ENODEV;

	int err = l4shmc_attach(shm_name, &ankh_shmarea);
	if (err)
		return err;

	err = l4shmc_rb_init_buffer(&_snd, &ankh_shmarea, "tx_ring", "tx_signal", bufsize);
	if (err)
		return err;
	err = l4shmc_rb_init_buffer(&_rcv, &ankh_shmarea, "rx_ring", "rx_signal", bufsize);
	if (err)
		return err;
	err = l4shmc_add_chunk(&ankh_shmarea, "info", sizeof(struct AnkhSessionDescriptor),
	                       &ankh_info_chunk);
	if (err)
		return err;

	L4::Ipc::Iostream s(l4_utcb());
	s << L4::Opcode(Ankh::Svc::Activate);
	l4_msgtag_t res = s.call(ankh_server.cap(), Ankh::Svc::Protocol);
	if (res.has_error())
		return res.label();

	printf("activated Ankh connection.\n");

	return 0;
}


L4_CV AnkhSessionDescriptor *l4ankh_get_info() L4_NOTHROW
{
	if (!ankh_conn_info)
		ankh_conn_info = static_cast<AnkhSessionDescriptor*>(l4shmc_chunk_ptr(&ankh_info_chunk));
	return ankh_conn_info;
}


#if 0
extern "C" __attribute__((weak)) void fixup_shm_caphandlers(
	int, l4shmc_area_t*, l4shmc_ringbuf_t*, l4shmc_ringbuf_t*);
#endif

L4_CV int l4ankh_prepare_send(l4_cap_idx_t owner) L4_NOTHROW
{
	int r = l4shmc_rb_attach_sender(&_snd, "tx_signal", owner);
	return r;
}


L4_CV int l4ankh_prepare_recv(l4_cap_idx_t owner) L4_NOTHROW
{
	l4shmc_rb_attach_receiver(&_rcv, owner);
#if 0
	if (fixup_shm_caphandlers)
		fixup_shm_caphandlers((int)&ankh_server, &ankh_shmarea, &_snd, &_rcv);
#endif
	return 0;
}


L4_CV int l4ankh_recv_blocking(char *buffer, unsigned *size) L4_NOTHROW
{
	if (L4_UNLIKELY(!buffer || !size))
		return -L4_EINVAL;

	int err;

	err = l4shmc_rb_receiver_wait_for_data(&_rcv, 1);
	if (err)
		return err;

	return l4shmc_rb_receiver_copy_out(L4SHMC_RINGBUF_HEAD(&_rcv),
	                                   buffer, size);
}


L4_CV int l4ankh_recv_nonblocking(char *buffer, unsigned *size) L4_NOTHROW
{
	if (L4_UNLIKELY(!buffer || !size))
		return -L4_EINVAL;

	int err;

	err = l4shmc_rb_receiver_wait_for_data(&_rcv, 0);
	if (!err)
		err = l4shmc_rb_receiver_copy_out(L4SHMC_RINGBUF_HEAD(&_rcv), buffer, size);

	return err;
}


L4_CV int l4ankh_send(char *data, unsigned size, char block) L4_NOTHROW
{
	int err = l4shmc_rb_sender_next_copy_in(&_snd, data, size, block);
	if (!err)
		l4shmc_rb_sender_commit_packet(&_snd);

	return err;
}
