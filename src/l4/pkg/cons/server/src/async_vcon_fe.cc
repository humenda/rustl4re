/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "async_vcon_fe.h"

#include <l4/re/error_helper>
#include <pthread.h>
#include <cstdio>

using L4Re::chksys;

void *Async_vcon_fe::setup()
{
  int err = l4_error(_vcon->bind(0, obj_cap()));

  if (err == -L4_EBADPROTO)
    printf("WARNING: frontend without input\n");

  // we just do not care about errors here too
  l4_vcon_attr_t attr = { 0, 0, 0 };
  _vcon->get_attr(&attr);
  attr.l_flags &= ~(L4_VCON_ECHO | L4_VCON_ICANON);
  attr.o_flags &= ~(L4_VCON_ONLCR | L4_VCON_OCRNL | L4_VCON_ONLRET);
  attr.o_flags |= L4_VCON_ONLCR;
  attr.i_flags &= ~(L4_VCON_INLCR | L4_VCON_IGNCR | L4_VCON_ICRNL);
  _vcon->set_attr(&attr);

  handle_pending_input();
  _initialized = true;
  return NULL;
}

void *Async_vcon_fe::_setup(void *_self)
{
  Async_vcon_fe *self = static_cast<Async_vcon_fe*>(_self);
  return self->setup();
}

Async_vcon_fe::Async_vcon_fe(L4::Cap<L4::Vcon> con, L4Re::Util::Object_registry *r)
: Vcon_fe_base(con, r), _initialized(false)
{
  pthread_t tid;
  pthread_create(&tid, NULL, Async_vcon_fe::_setup, this);
}

int
Async_vcon_fe::write(char const *buf, unsigned sz)
{
  if (!_initialized)
    return sz;

  return do_write(buf, sz);
}
