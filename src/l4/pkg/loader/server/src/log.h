/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/capability>
#include <l4/sys/vcon>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/cxx/string>

#include "local_service.h"
#include <cstring>

namespace Ldr {

class Log : public L4::Epiface_t<Log, L4::Vcon>
{
private:
  enum { Max_tag = 8 };
  char _tag[Max_tag];
  unsigned char _l;
  unsigned char _color;
  bool _in_line;

public:
  Log() : _l(0), _color(0), _in_line(false) {}
  void set_tag(cxx::String const &tag)
  {
    _l = cxx::min<unsigned long>(tag.len(), Max_tag);
    memcpy(_tag, tag.start(), _l);
  }

  void set_color(unsigned char color)
  { _color = color; }

  char const *tag() const { return _tag; }
  unsigned char color() const { return _color; }

  int op_bind(L4::Icu::Rights, l4_umword_t,
              L4::Ipc::Snd_fpage)
  { return -L4_ENOSYS; }

  int op_unbind(L4::Icu::Rights, l4_umword_t,
                L4::Ipc::Snd_fpage)
  { return -L4_ENOSYS; }
  int op_info(L4::Icu::Rights, L4::Icu::_Info &)
  { return -L4_ENOSYS; }
  int op_msi_info(L4::Icu::Rights, unsigned, l4_uint64_t,
                  l4_icu_msi_info_t &)
  { return -L4_ENOSYS; }
  int op_mask(L4::Icu::Rights, l4_umword_t)
  { return -L4_ENOSYS; }
  int op_unmask(L4::Icu::Rights, l4_umword_t)
  { return -L4_ENOSYS; }
  int op_set_mode(L4::Icu::Rights, l4_umword_t, l4_umword_t)
  { return -L4_ENOSYS; }

  l4_msgtag_t op_dispatch(l4_utcb_t *utcb, l4_msgtag_t tag, L4::Vcon::Rights);

  static int color_value(cxx::String const &col);
  virtual ~Log() {}
};
}
