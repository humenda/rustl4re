#include "session"
#include <string>
#include <algorithm>
#include <iostream>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>
#include <l4/ankh/shm>
#include <l4/ankh/packet_analyzer.h>
#include <l4/shmc/shmc.h>
#include <l4/sys/debugger.h>
#include <pthread-l4.h>

#include "linux_glue.h"

int Ankh::ServerSession::count = 0;

Ankh::ServerSession* Ankh::Session_factory::create(char *config)
{
#if 1
	std::cout << "Configuration: " << config << "\n";
#endif
	bool want_phys = false;
	bool promisc   = false;
	bool debug     = true;
	bool bcast     = true;
	char *devname = 0;
	char *shmname = 0;
	unsigned bufsize = 2048;
	std::vector<std::string> v;

	std::string s(config);
	boost::tokenizer<boost::escaped_list_separator<char> > tok(s);
	for (boost::tokenizer<boost::escaped_list_separator<char> >::iterator
	     beg = tok.begin(); beg != tok.end(); ++beg)
	{
		if (*beg == "debug") {
			std::cout << "  Debug mode ON.\n";
			debug = true;
		}
		else if (*beg == "promisc") {
			std::cout << "  Using promiscuous mode.\n";
			promisc = true;
		}
		else if (*beg == "phys_mac") {
			std::cout << "  Physical MAC requested.\n";
			want_phys = true;
		}
		else if (*beg == "nobroadcast") {
			std::cout << "  Disabling delivery of broadcast packets.\n";
			bcast = false;
		}
		else if (boost::starts_with(*beg, "device")) {
			boost::split(v, *beg, boost::is_any_of("="));
			std::cout << "  Device '" << v[1] << "' requested.\n";
			devname = strdup(v[1].c_str());
		}
		else if (boost::starts_with(*beg, "shm")) {
			boost::split(v, *beg, boost::is_any_of("="));
			std::cout << "  SHM area '" << v[1] << "' requested.\n";
			shmname = strdup(v[1].c_str());
		}
		else if (boost::starts_with(*beg, "bufsize")) {
			boost::split(v, *beg, boost::is_any_of("="));
			std::cout << "  Buffer size: " << v[1] << "\n";
			bufsize = atoi(v[1].c_str());
		}
	}

	if (debug)
		ankh_set_debug();

	if (!devname)
		devname = strdup("eth0");
	if (!shmname)
		shmname = strdup("shm_area");

	Ankh::ServerSession *ret = new Ankh::ServerSession(want_phys, promisc, debug,
	                                                   devname, shmname, bufsize, bcast);
	assert(ret);
	_sessions.push_back(ret);

	free(devname);
	free(shmname);

	return ret;
}


void Ankh::ServerSession::configure()
{
}


void Ankh::ServerSession::deliver(char *packet, unsigned len)
{
	int err = _recv_chunk->next_copy_in(packet, len, false);
	if (!err)
	{
		info()->num_rx++;
		info()->rx_bytes += len;
		_recv_chunk->commit_packet();
	}
	else
	{
		info()->rx_dropped++;
	}
}


void Ankh::ServerSession::generate_mac()
{
	unsigned char data[10];

	memcpy(data, _dev->hw_addr(), 6);
	memcpy(data + 6, &Ankh::ServerSession::count, sizeof(int));
	_mac[0] = 0x04;
	_mac[1] = 0xEA;
	unsigned checksum = Ankh::Util::adler32(data, 10);
	memcpy(_mac + 2, &checksum, sizeof(unsigned));

	Ankh::ServerSession::count++;
}


EXTERN_C void *xmit_thread_fn(void *data)
{
	Ankh::ServerSession *session = reinterpret_cast<Ankh::ServerSession*>(data);
	struct AnkhSessionDescriptor *sd = session->info();
	assert(sd);

	char namebuf[40];
	snprintf(namebuf, 40, "ankh.%s.xmit", session->_shmname);
	l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), namebuf);

	enable_ux_self();

	int _id =  l4_debugger_global_id(pthread_l4_cap(pthread_self()));
	std::cout << "xmit thread started, data @ " << data
	          << ", TID: 0x" << std::hex << _id << std::dec <<  "\n";


	// we need to init the xmit chunk member here, because we want to make
	// sure that _this_ thread is attached to the xmit signal
	session->_xmit_chunk = new Ankh::Shm_receiver(&session->_shm_area,
	                                              "tx_ring", "tx_signal");

	Ankh::Shm_receiver* chunk = session->_xmit_chunk;

	// make sure the client makes the same assumptions about shm
	// size as we do
	std::cout << chunk->buffer()->data_size() << " ... " << session->ringsize() << "\n";
	assert(chunk->buffer()->data_size() == session->ringsize());

	unsigned mtu = session->dev()->mtu();
	// we need a tx buffer that can (potentially) be used for DMA by the
	// underlying device driver
	char *tx_buf = static_cast<char*>(alloc_dmaable_buffer(mtu));

	while(true)
	{
		Ankh::ServerSession *s = 0;
		chunk->wait_for_data();

		unsigned size = mtu;
		int err = chunk->next_copy_out(tx_buf, &size);
		assert(err == 0);

		if (session->debug()) {
			packet_analyze(tx_buf, size);
		}

//		std::cout << "xmit " << (void*)tx_buf << " " << size << "\n";
		// try to deliver locally
		unsigned local = packet_deliver(tx_buf, size, session->dev()->name(), static_cast<unsigned>(true));

		if (!local || Ankh::Util::is_broadcast_mac(tx_buf))
		{
			err = session->dev()->transmit(tx_buf, size);
			chunk->notify_done();
		}

		// Stats update
		// if delivered locally or successfully sent through NIC...
		if (local || err == 0) {
			sd->num_tx++;
			sd->tx_bytes += size;
		}
		else
			sd->tx_dropped++;
	}
	return NULL;
}


void Ankh::ServerSession::init_shm_info()
{
	struct AnkhSessionDescriptor *sd = info();
	memcpy(sd->mac, _mac, 6);
	sd->mtu        = _dev->mtu();
	sd->num_tx     = 0UL;
	sd->tx_bytes   = 0UL;
	sd->tx_dropped = 0UL;
	sd->num_rx     = 0UL;
	sd->rx_bytes   = 0UL;
	sd->rx_dropped = 0UL;
}


int Ankh::ServerSession::shm_create()
{
	long err = l4shmc_attach(_shmname, &this->_shm_area);
	std::cout << "l4shmc_attach(\"" << _shmname << "\") = " << err << "\n";
	if (err)
		// XXX: shmc_destroy() ?
		return -L4_ENOMEM;

	/*
	 * Create info/stats area
	 */
	unsigned header_size = sizeof(struct AnkhSessionDescriptor);
	std::cout << "Shm_chunk::get(" << (void*)&_shm_area << ", \"info\", " << header_size << ")\n";
	_head_chunk = Ankh::Shm_chunk::get(&_shm_area, "info", header_size);
	std::cout << " ... = " << (void*)_head_chunk << "\n";
	assert(_head_chunk);
	init_shm_info();

	_recv_chunk = new Ankh::Shm_sender(&_shm_area, "rx_ring",
	                                   "rx_signal");
	assert(_recv_chunk);

	err = pthread_create(&_xmit_thread, NULL, xmit_thread_fn, this);
	assert(err == 0);

	return 0;
}


Ankh::ServerSession* Ankh::Session_factory::find_session_for_mac(
                         char const* mac, char const *dev, Ankh::ServerSession* prev)
{
	std::list<ServerSession*>::iterator iter;

	/* 
	 * When re-entering after having already found one session, we need
	 * to skip the previously checked ones.
	 */
	if (prev) {
		iter = std::find(_sessions.begin(), _sessions.end(), prev);
		if (iter == _sessions.end())
			return 0;
		iter++;
	}
	else
		iter = _sessions.begin();

	for ( ; iter != _sessions.end(); ++iter) {
		// skip inactive sessions
		if (!(*iter)->is_active())
			continue;
		// don't accept a packet that came through a different device
		if (strcmp(dev, (*iter)->dev()->name()))
			continue;
		// don't receive packets sent by ourselves
		if (memcmp((*iter)->mac(), mac + 6, 6) == 0)
			continue;
		// deliver broadcast packets only if client did not switch this off
		if (Ankh::Util::is_broadcast_mac(mac) && (*iter)->want_bcast())
			return *iter;
		// deliver packets with correct MAC
		if (!memcmp(mac, (*iter)->mac(), 6))
			return *iter;
	}

	return 0;
}
