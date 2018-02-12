/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/console>
#include <l4/re/env>
#include <l4/re/event>
#include <l4/re/event_enums.h>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/sys/err.h>
#include <l4/sys/kdebug.h>
#include <pthread-l4.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>

#include <readline/history.h>
#include <readline/readline.h>


#include <l4/libc_backends/libc_be_file.h>
#include <sys/ioctl.h>
#include <l4/event/event>


class in_ops : public l4file_ops
{
private:
  Event::Event_cap  _event;

public:
  in_ops() : l4file_ops(1, "IN ops"), _event(this) {}

  int get_irq(L4::Cap<L4::Irq> i)
  { return l4_error(L4Re::Env::env()->log()->get_irq(i)); }

  ssize_t read(void *buf, size_t len)
  {
    if (len == 0)
      return -EINVAL;

    int ret = L4Re::Env::env()->log()->read((char *)buf, len);
    if (ret > (int)len)
      ret = len;

    while (ret == 0)
      {
        // nothing read, read needs to block
        _event.wait();

        ret = L4Re::Env::env()->log()->read((char *)buf, len);
        if (ret > (int)len)
          ret = len;
      }

    return ret;
  }

  int ioctl(unsigned long request, void *argp)
  {
    switch (request) {
      case TCGETS:
	{
	  //vt100_tcgetattr(term, (struct termios *)argp);

	  struct termios *t = (struct termios *)argp;

          // XXX: well, we're cheating, get this from the other side!

	  t->c_iflag = 0;
	  t->c_oflag = 0; // output flags
	  t->c_cflag = 0; // control flags

	  t->c_lflag = 0; // local flags
	  t->c_lflag |= ECHO; // if term->echo
	  //t->c_lflag |= ICANON; // if term->term_mode == VT100MODE_COOKED

	  t->c_cc[VEOF]   = CEOF;
	  t->c_cc[VEOL]   = _POSIX_VDISABLE;
	  t->c_cc[VEOL2]  = _POSIX_VDISABLE;
	  t->c_cc[VERASE] = CERASE;
	  t->c_cc[VWERASE]= CWERASE;
	  t->c_cc[VKILL]  = CKILL;
	  t->c_cc[VREPRINT]=CREPRINT;
	  t->c_cc[VINTR]  = CINTR;
	  t->c_cc[VQUIT]  = _POSIX_VDISABLE;
	  t->c_cc[VSUSP]  = CSUSP;
	  t->c_cc[VSTART] = CSTART;
	  t->c_cc[VSTOP] = CSTOP;
	  t->c_cc[VLNEXT] = CLNEXT;
	  t->c_cc[VDISCARD]=CDISCARD;
	  t->c_cc[VMIN] = CMIN;
	  t->c_cc[VTIME] = 0;

	  //printf("TCGETS: c_lflags = %08x\n", t->c_lflag);

	}

	return 0;

      case TCSETS:
      case TCSETSW:
      case TCSETSF:
	{
          //vt100_tcsetattr(term, (struct termios *)argp);
	  struct termios *t = (struct termios *)argp;

          //XXX: probably we need to get this over to the other side!

	  printf("TCSETS*: c_lflags = %08x\n", t->c_lflag);
	}
        return 0;

      default:
        printf("my_ioctl: unknown %lx %p\n", request, argp);
	break;
    };
    return -EINVAL;
  }
};

static l4file_ops *get_in_ops()
{
  static in_ops _myops;
  return &_myops;
}

L4FILE_REGISTER_FILE(get_in_ops(), STDIN_FILENO);

