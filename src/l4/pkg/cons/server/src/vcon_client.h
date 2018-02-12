/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "client.h"
#include "server.h"

#include <l4/re/util/icu_svr>
#include <l4/re/util/vcon_svr>
#include <l4/re/util/object_registry>

class Vcon_client
: public L4::Epiface_t<Vcon_client, L4::Vcon, Server_object>,
  public L4Re::Util::Icu_cap_array_svr<Vcon_client>,
  public L4Re::Util::Vcon_svr<Vcon_client>,
  public Client
{
public:
  typedef L4Re::Util::Icu_cap_array_svr<Vcon_client> Icu_svr;
  typedef L4Re::Util::Vcon_svr<Vcon_client> My_vcon_svr;

  Vcon_client(std::string const &name, int color, size_t bufsz, Key key,
              L4Re::Util::Object_registry *)
  : Icu_svr(1, &_irq),
    Client(name, color, 512, bufsz < 512 ? _dfl_obufsz : bufsz, key)
  {}

  void vcon_write(const char *buffer, unsigned size) throw();
  unsigned vcon_read(char *buffer, unsigned size) throw();

  int vcon_set_attr(l4_vcon_attr_t const *a) throw();
  int vcon_get_attr(l4_vcon_attr_t *attr) throw();

  const l4_vcon_attr_t *attr() const { return &_attr; }

  void trigger() const { _irq.trigger(); }

  bool collected() { return Client::collected(); }

  static void default_obuf_size(unsigned bufsz)
  {
    _dfl_obufsz = cxx::max(512U, cxx::min(16U << 20, bufsz));
  }

private:
  enum { Default_obuf_size = 40960 };
  static unsigned _dfl_obufsz;
  Icu_svr::Irq _irq;
};
