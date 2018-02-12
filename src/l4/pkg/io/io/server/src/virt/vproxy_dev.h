/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include "vdevice.h"
#include "hw_device.h"
#include "hw_device_client.h"

namespace Vi {

class Proxy_dev : public Device, public Hw::Device_client
{
public:
  explicit Proxy_dev(Hw::Device *d);

  void dump(int indent) const  { Device::dump(indent); }
  bool check_conflict(Hw::Device_client const *other) const
  {
    if (Proxy_dev const *p = dynamic_cast<Proxy_dev const *>(other))
      return p->_hwd == _hwd;
    return false;
  }

  std::string get_full_name() const { return get_full_path(); }
  void notify(unsigned type, unsigned event, unsigned value)
  { Device::notify(type, event, value, true); }

  char const *hid() const { return _hwd->hid(); }

  virtual bool match_cid(cxx::String const &s) const
  { return _hwd->match_cid(s); }

  Io::Event_source_infos const *get_event_infos() const
  { return _hwd->get_event_infos(); }

private:
  Hw::Device *_hwd;
};

}
