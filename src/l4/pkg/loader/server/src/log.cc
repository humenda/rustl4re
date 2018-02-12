/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>,
 *               Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/log>
#include <l4/re/log-sys.h>
#include <l4/sys/kdebug.h>
#include <l4/cxx/minmax>

#include "log.h"
#include "global.h"
#include "obj_reg.h"

#include <cstdio>
#include <unistd.h>

static Ldr::Log *last_log = 0;

static void my_outnstring(char const *s, unsigned long len)
{
  write(1, s, len);
}

static void mycpy(char **buf, int *rem, char const *s, int l)
{
  int o = cxx::min(*rem, l);
  memcpy(*buf, s, o);
  *rem -= o;
  *buf += o;
}

static char msgbuf[4096];

l4_msgtag_t
Ldr::Log::op_dispatch(l4_utcb_t *utcb, l4_msgtag_t tag, L4::Vcon::Rights)
{
  if (tag.words() < 2)
    return l4_msgtag(-L4_EINVAL, 0, 0, 0);

  l4_msg_regs_t *m = l4_utcb_mr_u(utcb);
  L4::Opcode op = m->mr[0];

  // we only have one opcode
  if (op != L4Re::Log_::Print)
    return l4_msgtag(-L4_ENOSYS, 0, 0, 0);

  char *msg = Glbl::log_buffer;
  unsigned long len_msg = sizeof(Glbl::log_buffer);

  if (len_msg > (tag.words() - 2) * sizeof(l4_umword_t))
    len_msg = (tag.words() - 2) * sizeof(l4_umword_t);

  if (len_msg > m->mr[1])
    len_msg = m->mr[1];

  memcpy(msg, &m->mr[2], len_msg);

  int rem = sizeof(msgbuf);
  while (len_msg > 0 && msg[0])
    {
      char *obuf = msgbuf;
#if 1
      if (color())
	{
	  int n = snprintf(obuf, rem, "\033[%s3%dm",
	      (color() & 8) ? "01;" : "", (color() & 7));
	  obuf = obuf + n;
	  rem -= n;
	}
      else
	mycpy(&obuf, &rem, "\033[0m", 4);
#endif
      if (last_log != this)
	{
	  if (last_log != 0)
	    my_outnstring("\n", 1);

	  mycpy(&obuf, &rem, _tag, cxx::min<unsigned long>(_l, Max_tag));
	  if (_l < Max_tag)
	    mycpy(&obuf, &rem, "             ", Max_tag-_l);

	  if (_in_line)
	    mycpy(&obuf, &rem, ": ", 2);
	  else
	    mycpy(&obuf, &rem, "| ", 2);
	}

      unsigned long i;
      for (i = 0; i < len_msg; ++i)
	if (msg[i] == '\n' || msg[i] == 0)
	  break;

      mycpy(&obuf, &rem, msg, i);

      if (i <len_msg && msg[i] == '\n')
	{
          if (color())
            mycpy(&obuf, &rem, "\033[0m", 4);
	  mycpy(&obuf, &rem, "\n", 1);
	  _in_line = false;
	  last_log = 0;
	  ++i;
	}
      else
	{
	  last_log = this;
	  _in_line = true;
	}
      my_outnstring(msgbuf, obuf-msgbuf);

      msg += i;
      len_msg -= i;
    }

  if (_in_line && color())
    my_outnstring("\033[0m", 4);

  // and finally done
  return l4_msgtag(-L4_ENOREPLY, 0, 0, 0);
}


int
Ldr::Log::color_value(cxx::String const &col)
{
  int c = 0, bright = 0;

  if (col.empty())
    return 0;

  switch(col[0])
    {
    case 'N': bright = 1; /* FALLTHRU */ case 'n': c = 0; break;
    case 'R': bright = 1; /* FALLTHRU */ case 'r': c = 1; break;
    case 'G': bright = 1; /* FALLTHRU */ case 'g': c = 2; break;
    case 'Y': bright = 1; /* FALLTHRU */ case 'y': c = 3; break;
    case 'B': bright = 1; /* FALLTHRU */ case 'b': c = 4; break;
    case 'M': bright = 1; /* FALLTHRU */ case 'm': c = 5; break;
    case 'C': bright = 1; /* FALLTHRU */ case 'c': c = 6; break;
    case 'W': bright = 1; /* FALLTHRU */ case 'w': c = 7; break;
    default: c = 0;
    }

  return (bright << 3) | c;
}

