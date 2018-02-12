/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Björn Döbel <doebel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <l4/shmc/shmc.h>
#include <l4/shmc/ringbuf.h>
#include <l4/util/assert.h>
#include <l4/sys/debugger.h>
#include <l4/sys/kdebug.h>

/***************************
 *                         *
 *  GENERIC RINGBUF STUFF  *
 *                         *
 ***************************/

typedef struct 
{
#if L4SHMC_RINGBUF_POISONING
	char mag1;
#endif
	unsigned size;
#if L4SHMC_RINGBUF_POISONING
	char mag2;
#endif
} size_cookie_t;

#define SIZE_COOKIE_MAGIC1 (char)0xDE
#define SIZE_COOKIE_MAGIC2 (char)0xAD
#define EXTRACT_SIZE(size_cookie) (((size_cookie_t*)(size_cookie))->size)
#if L4SHMC_RINGBUF_POISONING
#define SIZE_COOKIE_INITIALIZER(size) (size_cookie_t){ SIZE_COOKIE_MAGIC1, (size), SIZE_COOKIE_MAGIC2 }
#else
#define SIZE_COOKIE_INITIALIZER(size) (size_cookie_t){(size)}
#endif

#if L4SHMC_RINGBUF_POISONING
#define ASSERT_COOKIE(c) \
	ASSERT_EQUAL(((size_cookie_t*)(c))->mag1, SIZE_COOKIE_MAGIC1); \
	ASSERT_EQUAL(((size_cookie_t*)(c))->mag2, SIZE_COOKIE_MAGIC2);
#else
#define ASSERT_COOKIE(c) do {} while(0)
#endif

#define BUF_HEAD_MAGIC1 (char)0xAB
#define BUF_HEAD_MAGIC2 (char)0xCD
#define BUF_HEAD_MAGIC3 (char)0xEF

#if L4SHMC_RINGBUF_POISONING
#define ASSERT_MAGIC(buf) \
	ASSERT_EQUAL(L4SHMC_RINGBUF_HEAD(buf)->magic1, BUF_HEAD_MAGIC1); \
	ASSERT_EQUAL(L4SHMC_RINGBUF_HEAD(buf)->magic2, BUF_HEAD_MAGIC2); \
	ASSERT_EQUAL(L4SHMC_RINGBUF_HEAD(buf)->magic3, BUF_HEAD_MAGIC3);
#else
#define ASSERT_MAGIC(buf) do {} while(0)
#endif


static void log_ringbuf_head(l4shmc_ringbuf_head_t *head)
{
	ASSERT_NOT_NULL(head);
	printf("   head @ %p, lock state '%s', data size %d\n", head,
	       head->lock == locked ? "LOCKED" : "unlocked",
	       head->data_size);
	printf("   next_rd %x next_wr %x filled %d, data @ %p\n",
	       head->next_read, head->next_write, head->bytes_filled,
	       head->data);
}


static void log_ringbuf(l4shmc_ringbuf_t *buf)
{
	ASSERT_NOT_NULL(buf);
	printf("buf @ %p, area %p (%s), size %d\n", buf, buf->_area,
	       buf->_area->_name, buf->_size);
	printf("   owner %lx, name %s, signal %s\n", buf->_owner,
	       buf->_chunkname, buf->_signame);
	log_ringbuf_head(L4SHMC_RINGBUF_HEAD(buf));
}

static void l4shmc_rb_generic_init(l4shmc_area_t *area,
                                  char const *chunk_name,
                                  l4shmc_ringbuf_t *buf)
{
	ASSERT_NOT_NULL(area);
	ASSERT_NOT_NULL(chunk_name);
	ASSERT_NOT_NULL(buf);

	buf->_area      = area;
	buf->_chunkname = strdup(chunk_name);
}


static void l4shmc_rb_generic_signal_init(l4shmc_ringbuf_t *buf, char const *sig_name)
{
	ASSERT_NOT_NULL(buf);
	buf->_signame   = strdup(sig_name);
}


static void l4shmc_rb_generic_deinit(l4shmc_ringbuf_t *buf)
{
	free(buf->_chunkname);
}


static void l4shmc_rb_generic_signal_deinit(l4shmc_ringbuf_t *buf)
{
	free(buf->_signame);
}


static void l4shmc_rb_receiver_get_signals(l4shmc_ringbuf_t *buf)
{
	ASSERT_NOT_NULL(buf);
	int err;

	static const int SIGNAME_SIZE = 40;
	char s1[SIGNAME_SIZE];
	strncpy(s1, buf->_signame, SIGNAME_SIZE);
	strncpy(s1 + strlen(buf->_signame), "_rx", 3);
	s1[strlen(buf->_signame) + 3] = 0;
	char s2[SIGNAME_SIZE];
	strncpy(s2, buf->_signame, SIGNAME_SIZE);
	strncpy(s2 + strlen(buf->_signame), "_tx", 3);
	s2[strlen(buf->_signame) + 3] = 0;

	err = l4shmc_get_chunk(buf->_area, buf->_chunkname, &buf->_chunk);
	ASSERT_OK(err);
	err = l4shmc_get_signal(buf->_area, s1, &buf->_signal_empty);
	ASSERT_OK(err);
	err = l4shmc_get_signal(buf->_area, s2, &buf->_signal_full);
	ASSERT_OK(err);

	printf("RCV: signal RX %lx, TX %lx\n",
		   l4_debugger_global_id(l4shmc_signal_cap(&buf->_signal_empty)),
		   l4_debugger_global_id(l4shmc_signal_cap(&buf->_signal_full)));
}


void l4shmc_rb_init_header(l4shmc_ringbuf_head_t *head);
/*static */void l4shmc_rb_init_header(l4shmc_ringbuf_head_t *head)
{
	ASSERT_NOT_NULL(head);

	head->lock         = unlocked;
	head->next_read    = 0;
	head->next_write   = 0;
	head->bytes_filled = 0;
	head->sender_waits = 0;
#if L4SHMC_RINGBUF_POISONING
	head->magic1 = BUF_HEAD_MAGIC1;
	head->magic2 = BUF_HEAD_MAGIC2;
	head->magic3 = BUF_HEAD_MAGIC3;
#endif
}

/***************************
 *                         *
 *    RINGBUF RECEIVER     *
 *                         *
 ***************************/
L4_CV int l4shmc_rb_init_receiver(l4shmc_ringbuf_t *buf, l4shmc_area_t *area,
                                  char const *chunk_name,
                                  char const *signal_name)
{
	l4shmc_rb_generic_init(area, chunk_name, buf);
	l4shmc_rb_generic_signal_init(buf, signal_name);

	l4shmc_rb_receiver_get_signals(buf);

	buf->_size = l4shmc_chunk_capacity(&buf->_chunk);
	printf("RCV: buf size %d\n", buf->_size);
	buf->_addr = l4shmc_chunk_ptr(&buf->_chunk);

	log_ringbuf(buf);
	return 0;
}


L4_CV void l4shmc_rb_attach_receiver(l4shmc_ringbuf_t *buf, l4_cap_idx_t owner)
{
	ASSERT_NOT_NULL(buf);
	ASSERT_VALID(owner);
	ASSERT_MAGIC(buf);

	int err;
	static const int SIGNAME_SIZE = 40;
	char s1[SIGNAME_SIZE];
	strncpy(s1, buf->_signame, SIGNAME_SIZE);
	strncpy(s1 + strlen(buf->_signame), "_rx", 3);
	s1[strlen(buf->_signame) + 3] = 0;

	buf->_owner = owner;
	printf("RCV: attaching to signal %lx (%lx)\n",
		   l4_debugger_global_id(l4shmc_signal_cap(&buf->_signal_empty)),
		   l4shmc_signal_cap(&buf->_signal_empty));
	err = l4shmc_attach_signal(buf->_area, s1, buf->_owner, &buf->_signal_empty);
#if 0
	printf("  ... attached: %d\n", err);
#endif
	ASSERT_OK(err);
}


L4_CV int l4shmc_rb_receiver_wait_for_data(l4shmc_ringbuf_t *buf, int block)
{
	ASSERT_NOT_NULL(buf);
	ASSERT_MAGIC(buf);

	l4_timeout_t to;

	if (block)
	    to = L4_IPC_NEVER;
	else
	    to = L4_IPC_BOTH_TIMEOUT_0;

	//while (!L4SHMC_RINGBUF_HEAD(buf)->bytes_filled)
	return l4shmc_wait_signal_to(&buf->_signal_empty, to);
}


L4_CV int l4shmc_rb_receiver_copy_out(l4shmc_ringbuf_head_t *head,
                                      char *target, unsigned *tsize)
{
	ASSERT_NOT_NULL(head);
	ASSERT_NOT_NULL(target);
	ASSERT_NOT_NULL(tsize);

	l4shmc_rb_lock(head);

	// users (e.g., L4Lx) may call this function directly w/o
	// checking whether data is available. In this case, simply
	// return an error here.
	if (head->bytes_filled == 0) {
		l4shmc_rb_unlock(head);
		return -L4_ENOENT;
	}

	char *addr     = head->data + head->next_read;
	char *max_addr = head->data + head->data_size;

	unsigned size_in_buffer = EXTRACT_SIZE(addr);
	ASSERT_COOKIE(addr);

	if (*tsize < size_in_buffer) {
		printf("tsize %d, psize %d\n", *tsize, size_in_buffer);
		printf("addr = %p\n", addr);
		return -L4_EINVAL;
	}
	ASSERT_GREATER_EQ(*tsize, size_in_buffer);

	*tsize = size_in_buffer;
	addr += sizeof(size_cookie_t);

	ASSERT_GREATER_EQ(max_addr, addr);

	if (addr + *tsize >= max_addr) {
		unsigned diff = head->data + head->data_size - addr;
		memcpy(target, addr, diff);
		memcpy(target + diff, head->data, *tsize - diff);
	}
	else
		memcpy(target, addr, *tsize);

	head->next_read += (*tsize + sizeof(size_cookie_t));
	// wrap around?
	if (head->next_read >= head->data_size)
		head->next_read %= head->data_size;
	// if there's no space for a length field, the sender will
	// have wrapped until beginning of buffer already
	if (head->next_read + sizeof(size_cookie_t) >= head->data_size)
		head->next_read = 0;
	head->bytes_filled -= (*tsize + sizeof(size_cookie_t));
	ASSERT_GREATER_EQ(head->data_size, head->bytes_filled);

	l4shmc_rb_unlock(head);

	return 0;
}


L4_CV void l4shmc_rb_receiver_notify_done(l4shmc_ringbuf_t *buf)
{
	ASSERT_NOT_NULL(buf);

	l4shmc_rb_lock(L4SHMC_RINGBUF_HEAD(buf));

	if (L4SHMC_RINGBUF_HEAD(buf)->sender_waits) {
		L4SHMC_RINGBUF_HEAD(buf)->sender_waits = 0;
#if 0
		printf("RCV: TRIGGER %lx (%lx)\n", buf->_signal_empty._sigcap,
			   l4_debugger_global_id(buf->_signal_empty._sigcap));
#endif
		l4shmc_trigger(&buf->_signal_full);
	}

	l4shmc_rb_unlock(L4SHMC_RINGBUF_HEAD(buf));
}


L4_CV int l4shmc_rb_receiver_read_next_size(l4shmc_ringbuf_head_t *head)
{
	ASSERT_NOT_NULL(head);

	int ret = -1;

	l4shmc_rb_lock(head);
	if (head->bytes_filled > 0) {
		char *addr = head->data + head->next_read;
		ASSERT_COOKIE(addr);
		ret = EXTRACT_SIZE(addr);
	}
	l4shmc_rb_unlock(head);

	return ret;
}

/***************************
 *                         *
 *    RINGBUF SENDER       *
 *                         *
 ***************************/

static L4_CV void l4shmc_rb_sender_add_signals(l4shmc_ringbuf_t *buf,
                                               char const *signal_name)
{
	int err;

	ASSERT_NOT_NULL(buf);
	ASSERT_NOT_NULL(signal_name);

	static const int SIGNAME_SIZE = 40;
	char s1[SIGNAME_SIZE];
	char s2[SIGNAME_SIZE];
	strncpy(s1, signal_name, SIGNAME_SIZE);
	strncpy(s1 + strlen(signal_name), "_rx", 3);
	s1[strlen(signal_name) + 3] = 0;
	strncpy(s2, signal_name, SIGNAME_SIZE);
	strncpy(s2 + strlen(signal_name), "_tx", 3);
	s2[strlen(signal_name) + 3] = 0;

	err = l4shmc_add_signal(buf->_area, s1, &buf->_signal_empty);
	ASSERT_OK(err);

	err = l4shmc_add_signal(buf->_area, s2, &buf->_signal_full);
	ASSERT_OK(err);

	printf("SND: signal RX %lx, TX %lx\n",
		   l4_debugger_global_id(l4shmc_signal_cap(&buf->_signal_empty)),
		   l4_debugger_global_id(l4shmc_signal_cap(&buf->_signal_full)));

}


L4_CV int l4shmc_rb_init_buffer(l4shmc_ringbuf_t *buf, l4shmc_area_t *area,
                                char const *chunk_name,
                                char const *signal_name, unsigned size)
{
	int err;

	l4shmc_rb_generic_init(area, chunk_name, buf);
	l4shmc_rb_generic_signal_init(buf, signal_name);

	buf->_size = size + sizeof(l4shmc_ringbuf_head_t);
	printf("add_chunk: area %p, name '%s', size %d\n", buf->_area,
		   buf->_chunkname, buf->_size);
	err = l4shmc_add_chunk(buf->_area, buf->_chunkname, buf->_size, &buf->_chunk);
	ASSERT_OK(err);

	l4shmc_rb_sender_add_signals(buf, signal_name);

	buf->_addr = l4shmc_chunk_ptr(&buf->_chunk);

	l4shmc_rb_init_header(L4SHMC_RINGBUF_HEAD(buf));
	L4SHMC_RINGBUF_HEAD(buf)->data_size = size;

	log_ringbuf(buf);

	return 0;
}


L4_CV int l4shmc_rb_attach_sender(l4shmc_ringbuf_t *buf, char const *signal_name,
                                  l4_cap_idx_t owner)
{
	ASSERT_NOT_NULL(buf);
	ASSERT_VALID(owner);
	int err;

	static const int SIGNAME_SIZE = 40;
	char signame[SIGNAME_SIZE];
	strncpy(signame, signal_name, SIGNAME_SIZE);
	strncpy(signame + strlen(signal_name), "_tx", 3);
	signame[strlen(signal_name) + 3] = 0;

	buf->_owner = owner;

	printf("SND: attaching to signal %lx (%lx)\n",
		   l4_debugger_global_id(l4shmc_signal_cap(&buf->_signal_full)),
		   l4shmc_signal_cap(&buf->_signal_full));
	printf("     buf @ %p\n", L4SHMC_RINGBUF_HEAD(buf));
	err = l4shmc_attach_signal(buf->_area, signame, buf->_owner, &buf->_signal_full);
	ASSERT_OK(err);

	return err;
}


L4_CV void l4shmc_rb_deinit_buffer(l4shmc_ringbuf_t *buf)
{
	ASSERT_NOT_NULL(buf);
	l4shmc_rb_generic_deinit(buf);
	l4shmc_rb_generic_signal_deinit(buf);
}


L4_CV char *l4shmc_rb_sender_alloc_packet(l4shmc_ringbuf_head_t *head, unsigned size)
{
	ASSERT_NOT_NULL(head);

	char *ret = NULL;
	int psize = size + sizeof(size_cookie_t); // need space to store packet size

	l4shmc_rb_lock(head);

	unsigned W = head->next_write;
	unsigned B = head->bytes_filled;
	
	int diff = head->data_size - B;
	ASSERT_GREATER_EQ(diff, 0);
	while (diff < psize)
	{
		head->sender_waits = 1;
		goto out;
	}

	// calculate pointer from offset and store packet len
	ret = head->data + W;
	*(size_cookie_t*)ret = SIZE_COOKIE_INITIALIZER(psize - sizeof(size_cookie_t));
	ret += sizeof(size_cookie_t);

	// make sure that there is at least enough space left at the end of
	// the ring so that we can store a whole packet length field
	// (sizeof(long))
	W += psize;
	// already larger
	if (W >= head->data_size)
		W %= head->data_size;
	// space does not fit another length field - step to beginning
	if (W + sizeof(size_cookie_t) >= head->data_size)
		W = 0;

	head->next_write = W;
	head->bytes_filled += psize;
	ASSERT_GREATER_EQ(head->data_size, head->bytes_filled);

out:
	l4shmc_rb_unlock(head);

	return ret;
}


L4_CV int l4shmc_rb_sender_next_copy_in(l4shmc_ringbuf_t *buf, char *data,
                                        unsigned size,
                                        int block_if_necessary)
{
	ASSERT_NOT_NULL(buf);
	ASSERT_NOT_NULL(data);
	ASSERT_MAGIC(buf);

	char *addr = 0;
	
#if 0
	printf("%s: %p %p %d %d\n", __func__, buf, data, size, block_if_necessary);
#endif
	while (((addr = l4shmc_rb_sender_alloc_packet(L4SHMC_RINGBUF_HEAD(buf), size)) == 0) &&
	         block_if_necessary) {
#if 0
		printf("%s: wait(%lx (%lx))\n", __func__, buf->_signal_full._sigcap,
			   l4_debugger_global_id(buf->_signal_full._sigcap));
		l4shmc_wait_signal(&buf->_signal_full);
#endif
	}

	if (addr) {
		l4shmc_rb_sender_put_data(buf, addr, data, size);
		return 0;
	}

	return -L4_ENOMEM;
}



L4_CV void l4shmc_rb_sender_put_data(l4shmc_ringbuf_t *buf, char *addr, char *data,
                                      unsigned dsize)
{
	ASSERT_NOT_NULL(buf);
	ASSERT_NOT_NULL(addr);
	ASSERT_NOT_NULL(data);
	ASSERT_MAGIC(buf);

	char *max_addr = L4SHMC_RINGBUF_DATA(buf) + L4SHMC_RINGBUF_DATA_SIZE(buf);
	if (max_addr < addr) {
		printf("ERROR: max %p, addr %p\n", (void*)max_addr, addr);
		printf("  DATA %p, DATA_SIZE %lx\n",
                       L4SHMC_RINGBUF_DATA(buf),
                       (unsigned long)L4SHMC_RINGBUF_DATA_SIZE(buf));
		log_ringbuf(buf);
	}
	ASSERT_GREATER_EQ(max_addr, addr);

	if (addr + dsize > max_addr)
	{
		l4_addr_t diff = (l4_addr_t)max_addr - (l4_addr_t)addr;
		memcpy(addr, data, diff);
		memcpy(L4SHMC_RINGBUF_DATA(buf), data + diff, dsize - diff);
	}
	else
	{
		memcpy(addr, data, dsize);
	}
}


L4_CV void l4shmc_rb_sender_commit_packet(l4shmc_ringbuf_t *buf)
{
	ASSERT_NOT_NULL(buf);
#if 0
	printf("SND: TRIGGER %lx (%lx)\n", buf->_signal_empty._sigcap,
		   l4_debugger_global_id(buf->_signal_empty._sigcap));
#endif
	long __attribute__((unused)) r = l4shmc_trigger(&buf->_signal_empty);
	ASSERT_OK(r);
}

