#include <iostream>
#include <cstdio>
#include <cassert>
#include <termios.h>
#include <cstring>
#include <unistd.h>

#include <l4/sys/kdebug.h>
#include <l4/re/env.h>

#include <l4/vbus/vbus.h>
#include <l4/vbus/vbus_types.h>
#include <l4/vbus/vbus_pci.h>

#include <l4/util/port_io.h>
#include <l4/util/util.h>

/* guess where i stole that... */
struct bootstrap_termios {
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


class Uart_x86 /*: public L4::Uart*/
{
	private:
		unsigned long _base;

	public:
		Uart_x86()
			: _base(~0UL)
		{
		}

		bool startup(unsigned long base)
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

			write("hello\n", 6);

			return true;
		}
		
		void shutdown() { }
		
		int get_char(bool blocking = true) const
		{
			int c = -1;

			if (!_base) { // base == 0 -> not started up yet
				enter_kdebug("!!");
				return c;
			}

			if (blocking) {
				while (!char_avail()) {
					l4_sleep(100);
				}
				c = l4util_in8(_base);
			} else {
				if (char_avail()) {
					c = l4util_in8(_base);
				}
			}

			return c;
		}


		int char_avail() const
		{
			return !!(l4util_in8(_base + 5) & 0x01);
		}

		inline void out_char(char c) const
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
		
		int write(char const *s, unsigned long count) const
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
};


class Uart_echo_server
{
	private:
		Uart_x86 _uart;
		char     _buf[80];
		unsigned _bufidx;

		void get_vbus_resources()
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


		void reset()
		{
			_bufidx = 0;
			memset(_buf, 0, sizeof(_buf));
		}

		void read_line()
		{
			int c;

			while (((c = _uart.get_char(true)) != '\r') &&
				   _bufidx <= sizeof(_buf)) {
				snprintf(_buf + _bufidx, sizeof(_buf), "%c", c);
				++_bufidx;
			}
		}


		void echo_line()
		{
			std::cout << _buf << std::endl;
			_uart.write(_buf, _bufidx);
			_uart.write("\n", 1);
		}

	public:
		Uart_echo_server()
			: _bufidx(0)
		{
			get_vbus_resources();
			std::cout << "==================" << std::endl;
			std::cout << "SERIAL ECHO SERVER" << std::endl;
			std::cout << "==================" << std::endl;
		}

		void run()
		{
			while (true) {
				reset();
				read_line();
				echo_line();
			}
		}
};


int main()
{
	Uart_echo_server srv;
	srv.run();
	return 0;
}
