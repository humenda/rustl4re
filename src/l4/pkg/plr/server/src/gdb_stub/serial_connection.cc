/*
 * gdb_stub/serial_connection.cc --
 *
 *     Connectivity through serial interface
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../log"
#include "gdbserver"
#include "connection"

#define MSG() DEBUGf(Romain::Log::Gdb)

#include <cstdio>
#include <cassert>
#include <termios.h>
#include <cstring>
#include <unistd.h>

#include <l4/sys/kdebug.h>
#include <l4/re/c/namespace.h>

#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vbus_pci.h>

#include <l4/util/port_io.h>
#include <l4/util/util.h>

/* guess where i stole that... */
static struct bootstrap_termios {
	unsigned long c_iflag;
	unsigned long c_oflag;
	unsigned long c_cflag;
	unsigned long c_lflag;
	unsigned char c_cc[20];
	long          c_ispeed;
	long          c_ospeed;

} serial_termios =
{
	0,            /* input flags */
	OPOST,        /* output flags */
	CS8,          /* control flags */
	0,            /* local flags */
	{ 'D'-64,     /* VEOF */
		_POSIX_VDISABLE,    /* VEOL */
		0,  
		'H'-64,     /* VERASE */
		0,  
		'U'-64,     /* VKILL */
		0,  
		0,  
		'C'-64,     /* VINTR */
		'\\'-64,        /* VQUIT */
		'Z'-64,     /* VSUSP */
		0,  
		'Q'-64,     /* VSTART */
		'S'-64,     /* VSTOP */
	},  
	115200,       /* input speed */
	115200,       /* output speed */
};


Uart_x86::Uart_x86()
: _base(~0UL)
{
}

bool Uart_x86::startup(unsigned long base)
{
	unsigned char dfr;
	unsigned divisor;

	/* Determine what to plug in the data format register.  */
	if (serial_termios.c_cflag & PARENB)
		if (serial_termios.c_cflag & PARODD)
			dfr = 0x08;
		else
			dfr = 0x18;
	else
		dfr = 0x00;
	if (serial_termios.c_cflag & CSTOPB)
		dfr |= 0x04;
	switch (serial_termios.c_cflag & 0x00000300)
	{
		case CS5: dfr |= 0x00; break;
		case CS6: dfr |= 0x01; break;
		case CS7: dfr |= 0x02; break;
		case CS8: dfr |= 0x03; break;
	}

	/* Convert the baud rate into a divisor latch value.  */
	divisor = 115200 / serial_termios.c_ospeed;

	/* Initialize the serial port.  */
	l4util_out8(0x80 | dfr,     _base + 3); /* DLAB = 1 */
	l4util_out8(divisor & 0xff, _base + 0);
	l4util_out8(divisor >> 8,   _base + 1);
	l4util_out8(0x03 | dfr,     _base + 3); /* DLAB = 0, frame = 8N1 */
	l4util_out8(0x00,           _base + 1); /* no interrupts enabled */
	l4util_out8(0x0b,           _base + 4); /* OUT2, RTS, and DTR enabled */

	l4util_out8(0x41, _base + 2);  /* 4 byte trigger + on */

	/* Clear all serial interrupts.  */
	l4util_in8(_base + 6);  /* ID 0: read RS-232 status register */
	l4util_in8(_base + 2);  /* ID 1: read interrupt identification reg */
	l4util_in8(_base + 0);  /* ID 2: read receive buffer register */
	l4util_in8(_base + 5);  /* ID 3: read serialization status reg */

	_base = base;

	write_uart("hello\n", 6);

	return true;
}

void Uart_x86::shutdown_uart() { }

int Uart_x86::get_char(bool blocking) const
{
	int c = -1;

	if (!_base) { // base == 0 -> not started up yet
		enter_kdebug("!!");
		return c;
	}

	if (!blocking)
		enter_kdebug("!blocking");

	if (blocking) {
		while (!char_avail()) {
			l4_sleep(10);
		}
		c = l4util_in8(_base);
	} else {
		if (char_avail()) {
			c = l4util_in8(_base);
		}
	}

	return c;
}


int Uart_x86::char_avail() const
{
	return !!(l4util_in8(_base + 5) & 0x01);
}

void Uart_x86::out_char(char c) const
{
	if (!_base) { // base == 0 -> not started up yet
		enter_kdebug("!!");
		return;
	}

	// poll until last char sent
	while (!(l4util_in8(_base + 5) & 0x20))
		;

	l4util_out8(c, _base);
}

int Uart_x86::write_uart(char const *s, unsigned long count) const
{
	unsigned long c = count;

	while (c) 
	{   
		if (*s == 10) 
			out_char(13);
		out_char(*s++);
		--c;
	}   
	return count;
}


void Romain::SerialConnection::get_vbus_resources()
{
	l4vbus_device_handle_t dev     = 0;
	l4vbus_device_t        devinfo;
	l4_cap_idx_t           vbus    = l4re_env_get_cap("vbus");
	l4vbus_resource_t      res;
	int                    err;

	err = l4vbus_get_device_by_hid(vbus, 0, &dev, "PNP0600", 0, &devinfo);
	assert(err == 0);
	assert(devinfo.num_resources == 1);

	err = l4vbus_get_resource(vbus, dev, 0, &res);
	assert(err == 0);

	err = l4vbus_request_resource(vbus, &res, 0);
	assert(err == 0);

	if (res.type == L4VBUS_RESOURCE_PORT) {
		_uart.startup(res.start);
	} else {
		enter_kdebug("resource not a port!?");
	}
}



void Romain::SerialConnection::setup_and_wait()
{
	// wait until we see the first character on the serial line
	while (!_uart.char_avail())
		;
}

void Romain::SerialConnection::disconnect()
{
}


int Romain::SerialConnection::wait_for_cmd(char * const buf, unsigned bufsize)
{
	buf[0] = _uart.get_char();
	//MSG() << "next packet start: " << buf[0] << "(" << std::hex << (unsigned)buf[0] << ")";
	switch(buf[0]) {
		case GDBInterrupt:
		case GDBAck:
		case GDBRetry:
			return 1;

		case GDBCmdStart:
			break;

		default:
			ERROR() << "Unknown cmd start? '" << buf[0] << "'";
			break;
	}

	unsigned idx = 1;
	while (idx < bufsize-2) {
		buf[idx] = _uart.get_char();
		//MSG() << buf[idx];
		if (buf[idx] == '#') {
			buf[idx+1] = _uart.get_char();
			buf[idx+2] = _uart.get_char();
			break;
		}
		++idx;
		//l4_sleep(1); // XXX sleep synchronization is evil. why does this not work without?
	}

	if (idx >= bufsize) {
		ERROR() << "Overflow in cmd buffer: " << buf;
		return -1;
	}

	//Romain::dump_mem(buf, 40);
	return bufsize;
}

int Romain::SerialConnection::senddata(char const * const buf, unsigned bufsize)
{
	MSG() << buf;
	return _uart.write_uart(buf, bufsize);
}
