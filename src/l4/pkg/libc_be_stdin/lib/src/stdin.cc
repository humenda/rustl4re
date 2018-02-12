/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/l4re_vfs/backend>
#include <l4/event/event>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

namespace {

using namespace L4Re::Vfs;

class in_ops : public Be_file_stream
{
private:
  Event::Event_cap _event;
  L4::Cap<L4::Vcon> _s;

public:
  in_ops(L4::Cap<L4::Vcon> v)  throw()
  : Be_file_stream(), _event(this), _s(v) {}
  ~in_ops() throw() {}

  int bind_irq(unsigned irq, L4::Cap<L4::Triggerable> const &irq_cap) throw()
  { return l4_error(L4Re::Env::env()->log()->bind(irq, irq_cap)); }

  ssize_t readv(struct iovec const *iov, int cnt) throw()
  {
    if (cnt == 0)
      return -EINVAL;

    int len = iov[0].iov_len;
    char *buf = (char *)iov[0].iov_base;
    if (len == 0)
      return -EINVAL;

    int ret = _s->read((char *)buf, len);
    if (ret > (int)len)
      ret = len;

    while (ret == 0)
      {
        // nothing read, read needs to block
        _event.wait();

        ret = _s->read((char *)buf, len);
        if (ret > (int)len)
          ret = len;
      }

    return ret;
  }

  int ioctl(unsigned long request, va_list args) throw()
  {
    switch (request) {
      case TCGETS:
	{
	  //vt100_tcgetattr(term, (struct termios *)argp);

	  struct termios *t = va_arg(args, struct termios *);

          l4_vcon_attr_t l4a;
          if (!l4_error(_s->get_attr(&l4a)))
            {
              t->c_iflag = l4a.i_flags;
              t->c_oflag = l4a.o_flags; // output flags
              t->c_cflag = 0; // control flags
              t->c_lflag = l4a.l_flags; // local flags
            }
          else
            t->c_iflag = t->c_oflag = t->c_cflag = t->c_lflag = 0;
#if 0
	  //t->c_lflag |= ECHO; // if term->echo
	  t->c_lflag |= ICANON; // if term->term_mode == VT100MODE_COOKED
#endif

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
	  struct termios *t = va_arg(args, struct termios *);

          //XXX: probably we need to get this over to the other side!

	  //printf("TCSETS*: c_lflags = %08x\n", t->c_lflag);

          l4_vcon_attr_t l4a;
          l4a.i_flags = t->c_iflag;
          l4a.o_flags = t->c_oflag; // output flags
          l4a.l_flags = t->c_lflag; // local flags
          _s->set_attr(&l4a);
	}
        return 0;

      default:
        printf("libc_be_stdin: ioctl: unknown request: %lx\n", request);
	break;
    };
    return -EINVAL;
  }

  int fstat64(struct stat64 *buf) const throw()
  {
    (void)buf;
    return 0;
  }
};



static void get_in_ops() __attribute__((constructor));
static void get_in_ops()
{
  static in_ops _myops(L4Re::Env::env()->log());
  _myops.add_ref(); // prevent the static object from being deleted
  L4Re::Vfs::vfs_ops->set_fd(STDIN_FILENO, cxx::ref_ptr(&_myops));
}

}
