/*
 * gdb_stub/tcp_connection.cc --
 *
 *     Connectivity through TCP/IP
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../log"
#include "../exceptions"
#include "connection"

#include <l4/util/util.h>

#define MSG() DEBUGf(Romain::Log::Gdb)

void Romain::TCPConnection::setup_and_wait()
{
	/* XXX make configurable */
	char const *argv[] =  {"--shm","shm","--bufsize","16384",0};
	int argc = 4;
	static int firstTime = 0;

	if (!firstTime) {
		/* Initialize lwIP NIC */
		l4_lwip_init(&argc,(char **)argv);

		while(!netif_is_up(&ankh_netif)) {
			MSG() << "waiting for DHCP to finish...";
			l4_sleep(2000);
		}

		MSG() << "lwIP init done.";
	}

	/*
	 * Create socket and wait for connection
	 */
	_inet.sin_len = 0;
	_inet.sin_family = AF_INET;
	_inet.sin_port = htons(port());
	_inet.sin_addr.s_addr = ankh_netif.ip_addr.addr;

	_socket = socket(PF_INET, SOCK_STREAM, 0);
	MSG() << "socket(): " << _socket;
	_check(_socket == -1, "socket()");

	int err = bind(_socket, (struct sockaddr*)&_inet, sizeof(_inet));
	MSG() << "bound: " << err;
	_check(err < 0, "bind()");

	err = listen(_socket, 10);
	MSG() << "listen: " << err;
	_check(err == -1, "listen()");

	socklen_t size = sizeof(_clnt);
	_socket_fd = accept(_socket, (struct sockaddr*)&_clnt, &size);
	MSG() << "accept: " << _socket_fd;
	_check(_socket_fd == -1, "accept()");
}


void Romain::TCPConnection::disconnect()
{
	//close(_socket_fd);
	close(_socket);
	memset(&_inet, 0, sizeof(_inet));
	memset(&_clnt, 0, sizeof(_clnt));
}


int Romain::TCPConnection::wait_for_cmd(char *const packetbuf, unsigned bufsize)
{
	int readBytes = 0;

	memset(packetbuf, 0, bufsize);

	MSG() << "reading from remote";
	readBytes = read(_socket_fd, packetbuf, bufsize);
	if (readBytes <= 0) {
		printf("read_socket(): %d\n", readBytes);
		return -1;
	}

	return readBytes;
}


int Romain::TCPConnection::senddata(char const * const buf, unsigned size)
{
	return ::write(_socket_fd, buf, size);
}
