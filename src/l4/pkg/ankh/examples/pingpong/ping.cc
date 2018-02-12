#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/cxx/ipc_stream>
#include <l4/ankh/protocol>
#include <l4/ankh/shm>
#include <l4/ankh/session>
#include <l4/util/util.h>
#include <l4/shmc/shmc.h>
#include <boost/format.hpp>
#include <iostream>
#include <cstring>
#include <l4/sys/debugger.h>
#include <pthread-l4.h>
#include <cstdlib>

Ankh::Shm_receiver *recv;
Ankh::Shm_sender   *send;
Ankh::Shm_chunk    *info;
char *shm_name;

int cfg_shmsize = 2048;
l4shmc_area_t ankh_shmarea;

#define mac_fmt       "%02X:%02X:%02X:%02X:%02X:%02X"
#define mac_str(mac)  (unsigned char)((mac)[0]), (unsigned char)((mac)[1]), \
                      (unsigned char)((mac)[2]), (unsigned char)((mac)[3]), \
                      (unsigned char)((mac)[4]), (unsigned char)((mac)[5])

static const int print_rate = 30000;
unsigned char const bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*
 * Print a MAC formatted
 */
static void print_mac(unsigned char *macptr)
{
	char macbuf_size = 32;
	char mac_buf[macbuf_size];
	snprintf(mac_buf, macbuf_size, mac_fmt, mac_str(macptr));
	boost::format f("%1$-18s");
	f % &mac_buf[0];
	std::cout << f;
}


static char *human_readable_size(unsigned size) {
	char sizebuf[40];

	if (size < 1000)
		snprintf(sizebuf, sizeof(sizebuf), "%d B", size);
	else if (size < 1000000)
		snprintf(sizebuf, sizeof(sizebuf), "%.02f kiB", size/1000.0);
	else if (size < 1000000000)
		snprintf(sizebuf, sizeof(sizebuf), "%.02f MiB", size/1000000.0);
	else
		snprintf(sizebuf, sizeof(sizebuf), "%.02f GiB", size/1000000000.0);

	return strdup(sizebuf);
}


/*
 * Print my device's stats
 */
static void print_stats()
{
	struct AnkhSessionDescriptor *sd = reinterpret_cast<struct AnkhSessionDescriptor*>(info->addr());

	std::cout << "info @ " << (void*)sd << "\n";
	print_mac(&sd->mac[0]); std::cout << "MTU " << sd->mtu << "\n";
	std::cout << "RX packets: " << sd->num_rx << " dropped: "    << sd->rx_dropped << "\n";
	std::cout << "TX packets: " << sd->num_tx << " dropped: "    << sd->tx_dropped << "\n";
	char *s1 = human_readable_size(sd->rx_bytes);
	char *s2 = human_readable_size(sd->tx_bytes);
	std::cout << "RX bytes: "  << s1 
	          << " TX bytes: " << s2 << "\n";
	free(s1); free(s2);
}

static void ankh_activate()
{
	L4::Cap<void> ankh_server;

	if (!(ankh_server = L4Re::Env::env()->get_cap<void>("ankh"))) {
		printf("Could not find Ankh server.\n");
		assert(false);
	}

	L4::Ipc::Iostream s(l4_utcb());

	s << L4::Opcode(Ankh::Svc::Activate);

	l4_msgtag_t res = s.call(ankh_server.cap(), Ankh::Svc::Protocol);
	ASSERT_EQUAL(l4_ipc_error(res, l4_utcb()), 0);

	printf("activated Ankh connection.\n");
}


static void get_shm_area(L4::Cap<void> /*session*/)
{
	int err = l4shmc_attach(shm_name, &ankh_shmarea);
	ASSERT_OK(err);
	std::cout << "Attached shm area '" << shm_name << "': " << err << "\n";

	Ankh::Shm_ringbuffer *send_rb = Ankh::Shm_ringbuffer::create(
	                                   &ankh_shmarea, "tx_ring",
	                                   "tx_signal", cfg_shmsize);
	Ankh::Shm_ringbuffer *recv_rb = Ankh::Shm_ringbuffer::create(
	                                   &ankh_shmarea, "rx_ring",
	                                   "rx_signal", cfg_shmsize);
	Ankh::Shm_chunk *info_chunk   = Ankh::Shm_chunk::create(&ankh_shmarea,
	                                   "info", sizeof(AnkhSessionDescriptor));

	std::cout << "SEND RB @ " << (void*)send_rb
	          << " RECV RB @ " << (void*)recv_rb
	          << " INFO @ " << (void *)info_chunk << "\n";

	ankh_activate();

	send = new Ankh::Shm_sender(&ankh_shmarea, "tx_ring",
	                            "tx_signal");
	std::cout << "sender: " << (void*)send << "\n";
	recv = new Ankh::Shm_receiver(&ankh_shmarea, "rx_ring",
	                              "rx_signal");
	std::cout << "receiver: " << (void*)recv << "\n";

	info = Ankh::Shm_chunk::get(&ankh_shmarea, "info",
	                            sizeof(struct AnkhSessionDescriptor));

	struct AnkhSessionDescriptor *sd = reinterpret_cast<struct AnkhSessionDescriptor*>(info->addr());

	std::cout << "Got assigned MAC: ";
	print_mac(sd->mac);
	std::cout << "\n";
	print_stats();
}


#if 0
#define DEBUG(x) \
	std::cout << x;
#else
#define DEBUG(x) {}
#endif

typedef struct {
	unsigned char _1[4];
	unsigned char txbuf[1024];
	unsigned char _2[4];
	unsigned char rxbuf[1024];
	unsigned char _3[4];
	const unsigned char packet_const;
	unsigned char _4[4];
} rx_struct;

rx_struct recv_buffer = {
	{0xAA, 0xAA, 0xAA, 0xAA},
	{0, },
	{0xBB, 0xBB, 0xBB, 0xBB},
	{0, },
	{0xCC, 0xCC, 0xCC, 0xCC},
	0x1B,
	{0xDD, 0xDD, 0xDD, 0xDD}
};


static void
generate_packet(unsigned char *buf,
				unsigned char dest[6],
				unsigned char src[6],
				unsigned char payload,
				unsigned size)
{
	memcpy(buf, dest, 6);
	memcpy(buf+6, src, 6);
	memset(buf+12, payload, size);
}

static void send_packet(Ankh::Shm_sender *snd,
						unsigned char *packet,
						unsigned size)
{
	DEBUG("sending()\n");
	int err = snd->next_copy_in((char*)packet, size);
	assert(err == 0);
	DEBUG("commit()\n");
	snd->commit_packet();
}

static void recv_packet(Ankh::Shm_receiver *rcv,
						unsigned char *buf,
						unsigned *size)
{
	DEBUG("recv()\n");
	recv->wait_for_data();
	DEBUG("got data.\n");
	int err = rcv->next_copy_out((char*)buf, size);
	assert(err == 0);
	DEBUG("copied out\n");
}


static void sanity_check_packet(unsigned char *buf,
						 unsigned char src[6],
						 unsigned char dest[6],
						 unsigned char payload,
						 unsigned size)
{
	if (memcmp(buf, dest, 6)) {
		std::cout << "Dest mismatch in buf @ " << (void*)buf << " ";
		print_mac(buf);
		std::cout << " <-> ";
		print_mac(dest);
		std::cout << std::endl;
		assert(memcmp(buf, dest, 6) == 0);
	}
	if (memcmp(buf + 6, src, 6)) {
		std::cout << "Src mismatch in buf @ " << (void*)buf << " ";
		print_mac(buf+6);
		std::cout << " <-> ";
		print_mac(src);
		std::cout << std::endl;
		assert(memcmp(buf+6, src, 6) == 0);
	}

	unsigned char *c = buf + 12;
	for ( ; c < buf + size; ++c)
		assert(*c == payload);
}

static void morping()
{
	unsigned char mac[6]       = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	unsigned char mymac[6]     = {0x00};
	unsigned bufsize = sizeof(recv_buffer.txbuf);
	// minimum size for sending test packets
	int min_size = 20;
	// maximum random element in size: bufsize - header len - minimum size
	int data_size = bufsize - 12 - min_size; 
	unsigned size;

	memcpy(mymac, reinterpret_cast<struct AnkhSessionDescriptor*>(info->addr())->mac, 6);

	l4_sleep(100);
	printf("MY MAC: "); print_mac(mymac); printf("\n");
	for (int cnt = 0; ; cnt++)
	{
		/*
		 * random data size:
		 * minimum_size <= rand <= data_size
		 */
		int rand = random() % data_size + min_size;

		generate_packet(recv_buffer.txbuf, mac, mymac, recv_buffer.packet_const, rand);

		size = rand + 12; // data + sizeof(header)
		send_packet(send, &recv_buffer.txbuf[0], size);

		size = bufsize;
		recv_packet(recv, &recv_buffer.rxbuf[0], &size);

		/*
		 * This task will always receive second. This means,
		 * the packet will contain valid MACs, but our own
		 * representation of the partner's MAC will be bcast.
		 * Therefore, we extract the partner's MAC from the
		 * packet's sender field.
		 */
		if (memcmp(mac, bcast_mac, 6) == 0)
			memcpy(mac, recv_buffer.rxbuf + 6, 6);
		else
			sanity_check_packet(recv_buffer.rxbuf, mac, mymac, recv_buffer.packet_const, rand);

		if (cnt > print_rate) {
			print_stats();
			cnt = 0;
		}
	}
}

static void morpong()
{
	unsigned char mac[6]       = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	unsigned char mymac[6]     = {0x00};
	unsigned bufsize = sizeof(recv_buffer.txbuf);
	unsigned size;

	memcpy(mymac, reinterpret_cast<struct AnkhSessionDescriptor*>(info->addr())->mac, 6);

	printf("MY MAC: "); print_mac(mymac); printf("\n");
	for (int cnt = 0; ; cnt++)
	{
		size = bufsize;
		recv_packet(recv, &recv_buffer.rxbuf[0], &size);

		/*
		 * This task will always recv() first, so the recipient
		 * MAC of the first packet will be a bcast one. If we
		 * find this, we extract the partner's MAC from the sender
		 * field.
		 */
		if (memcmp(recv_buffer.rxbuf, bcast_mac, 6) == 0)
			memcpy(mac, recv_buffer.rxbuf + 6, 6);
		else
			sanity_check_packet(recv_buffer.rxbuf, mac, mymac, recv_buffer.packet_const, size);

		generate_packet(recv_buffer.txbuf, mac, mymac, recv_buffer.packet_const, size-12);

		send_packet(send, &recv_buffer.txbuf[0], size);

		if (cnt > print_rate+1) {
			print_stats();
			cnt = 0;
		}
	}
}


static void setup_debug_names(std::string const &name)
{
	std::cout << "I am " << name << "\n";

	std::cout << "My TID = 0x" << std::hex << l4_debugger_global_id(pthread_l4_cap(pthread_self()))
	          << std::dec << "\n";
	//std::cout << "Ready to contact Ankh.\n";
	char const *c = name.c_str();
	for ( ; *c++ != '/'; ) ;

	std::cout << c << "\n";
	l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), c);

}


int main(int argc, char **argv)
{
	if (argc > 1) {
		std::cout << "Using shm area: '" << argv[1] << "'\n";
		shm_name = strdup(argv[1]);
	}

	L4::Cap<void> ankh_session;
	if (!(ankh_session = L4Re::Env::env()->get_cap<void>("ankh")))
	{
		std::cout << "Ankh not found.\n";
		return 1;
	}

	get_shm_area(ankh_session);

	std::string name(argv[0]);
	std::cout << name << "\n";
	setup_debug_names(name);
	std::cout << "txbuf @ " << (void*)recv_buffer.txbuf << ", rxbuf @ " << (void*)recv_buffer.rxbuf << std::endl;

	if (name == "rom/morping")
		morping();
	else
		morpong();

	l4_sleep_forever();
	return 0;
}
