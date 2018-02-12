/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "client.h"

#include <cstring>
#include <time.h>

Client::Client(std::string const &tag, int color, int rsz, int wsz, Key key)
: idx(0), _col(color), _tag(tag), _p(false), _keep(false),
  _timestamp(false), _new_line(true), _dead(false),
  _key(key), _wb(wsz), _rb(rsz), _output(0)
{
  _attr.i_flags = L4_VCON_ICRNL;
  _attr.o_flags = L4_VCON_ONLRET | L4_VCON_ONLCR;
  _attr.l_flags = L4_VCON_ECHO;
}

bool
Client::collected()
{
  _dead = true;
  if (_keep)
    return false;

  return true;
}

void
Client::print_timestamp()
{
  time_t t = time(NULL);
  struct tm *tt = localtime(&t);
  char b[25];

  if (int l = strftime(b, sizeof(b), "[%d %b %y %T] ", tt))
    wbuf()->put(b, l);
}

void
Client::cooked_write(const char *buf, long size) throw()
{
  if (size < 0)
    size = strlen(buf);

  Client::Buf *w = wbuf();
  Buf::Index pos = w->head();

  while (size--)
    {
      if (_new_line && timestamp())
        print_timestamp();

      char c = *buf++;

      if (_attr.o_flags & L4_VCON_ONLCR && c == '\n')
        w->put('\r');

      if (_attr.o_flags & L4_VCON_OCRNL && c == '\r')
        c = '\n';

      if (_attr.o_flags & L4_VCON_ONLRET && c == '\r')
        continue;

      w->put(c);

      _new_line = c == '\n';
    }

  if (_output)
    {
      w->write(pos, w->head(), this);
      if (!(_attr.l_flags & L4_VCON_ICANON))
        _output->flush(this);
    }
}
