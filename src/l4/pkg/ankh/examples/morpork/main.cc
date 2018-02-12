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

Ankh::Shm_receiver *recv;
Ankh::Shm_sender   *send;
Ankh::Shm_chunk    *info;
char *shm_name;
l4shmc_area_t ankh_shmarea;

static int cfg_shmsize = 16384;

#define mac_fmt       "%02X:%02X:%02X:%02X:%02X:%02X"
#define mac_str(mac)  (unsigned char)((mac)[0]), (unsigned char)((mac)[1]), \
                      (unsigned char)((mac)[2]), (unsigned char)((mac)[3]), \
                      (unsigned char)((mac)[4]), (unsigned char)((mac)[5])

char recvbuf[256];
char sendbuf[256];

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


/*
 * Print my device's stats
 */
static void print_stats()
{
	struct AnkhSessionDescriptor *sd
	  = reinterpret_cast<struct AnkhSessionDescriptor*>(info->addr());

	std::cout << "info @ " << (void*)sd << "\n";
	print_mac(&sd->mac[0]); std::cout << "MTU " << sd->mtu << "\n";
	std::cout << "RX packets: " << sd->num_rx << " dropped: "    << sd->rx_dropped << "\n";
	std::cout << "TX packets: " << sd->num_tx << " dropped: "    << sd->tx_dropped << "\n";
	std::cout << "RX bytes: "   << sd->rx_bytes << " TX bytes: " << sd->tx_bytes << "\n";
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


extern "C" int handle_icmp_packet(char *packet, unsigned size, char *buffer);
static void answer()
{
	unsigned bufsize = sizeof(recvbuf);
	memset(recvbuf, 0, sizeof(recvbuf));
	memset(sendbuf, 0, sizeof(sendbuf));

	int err = recv->next_copy_out(&recvbuf[0], &bufsize);
	assert(err == 0);

	err = handle_icmp_packet(&recvbuf[0], bufsize, &sendbuf[0]);
	if (err == 0)
	{
		if (!send->next_copy_in(&sendbuf[0], bufsize))
			send->commit_packet();
	}
}


static void receive()
{
	static int cnt = 0;
	while (true)
	{
		recv->wait_for_data();
		answer();
		recv->notify_done();

		if (cnt++ > 5000) {
			print_stats();
			cnt = 0;
		}
	}
}

static void rxtx()
{
	unsigned short cnt = 0;
	unsigned bufsize = 200;
	unsigned char txbuf[bufsize];
	unsigned char rxbuf[bufsize];

	for (;;++cnt) {
		if (cnt % 5000 == 0) {
			std::cout << "Still here.\n";
			cnt = 0;
		}
		/*
		 * generate
		 */
		memcpy(txbuf, info->addr(), 6);
		memset(txbuf+6, 0xAB, bufsize-6);

		/*
		 * send
		 */
		int err = send->next_copy_in((char*)&txbuf[0], bufsize);
		assert(err == 0);
		send->commit_packet();

		/*
		 * recv
		 */
		unsigned size = bufsize;
		recv->wait_for_data();
		err = recv->next_copy_out((char*)&rxbuf[0], &size);
		assert(err == 0);

		/*
		 * check identity
		 */
		if (!size == bufsize)
			std::cout << "SIZE: " << size << ", BUFSIZE: " << bufsize << "\n";
		assert(size == bufsize);
		if (memcmp(&rxbuf[0], &txbuf[0], bufsize))
		{
			std::cout << "TX:\n";
			for (unsigned x = 0; x < bufsize; ++x) {
				printf("%02lX ", (unsigned long)(txbuf[x]));
				if (x % 15 == 14) printf("\n");
			}
			printf("\n");
			std::cout << "RX:\n";
			for (unsigned x = 0; x < bufsize; ++x) {
				printf("%02lX ", (unsigned long)(rxbuf[x]));
				if (x % 15 == 14) printf("\n");
			}
			printf("\n");
		}
		assert(memcmp(&rxbuf[0], &txbuf[0], bufsize) == 0);

		/*
		 * cleanup
		 */
		memset(&rxbuf[0], 0, bufsize);
		memset(&txbuf[0], 0, bufsize);
	}
}


int main(int argc, char **argv)
{
    /*
     * XXX: getopt() !
     */
	if (argc > 1) {
		std::cout << "Using shm area: '" << argv[1] << "'\n";
		shm_name = strdup(argv[1]);
	}

	if (argc > 2) {
		for (int i=2; i < argc; ++i) {
			printf("argv[%d] = '%s'\n", i, argv[i]);
			if (!strcmp(argv[i], "-bufsize"))
			{
				cfg_shmsize = atoi(argv[i+1]);
				i++;
				printf("Using buf size %d\n", cfg_shmsize);
			}
		}
	}

	L4::Cap<void> ankh_session;
	if (!(ankh_session = L4Re::Env::env()->get_cap<void>("ankh")))
	{
		std::cout << "Ankh not found.\n";
		return 1;
	}

	int global_id = l4_debugger_global_id(pthread_l4_cap(pthread_self()));
	std::cout << "My TID = 0x" << std::hex << global_id << std::dec << "\n";
	l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), "morpork.main");
	//std::cout << "Ready to contact Ankh.\n";

	get_shm_area(ankh_session);

	if (1)
		receive();
	else
		rxtx();

	l4_sleep_forever();
	return 0;
}
