/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/re/video/goos>

#include <l4/mag/server/plugin>
#include <l4/mag/server/user_state>
#include <l4/sys/cxx/ipc_epiface>

namespace Mag_server {

class Service :
  public L4::Epiface_t<Service, L4::Factory, Object>,
  private Plugin
{
private:
  Core_api const *_core;

protected:
  User_state *ust() const { return _core->user_state(); }
  Registry *reg() const { return _core->registry(); }

public:
  Service(char const *name) : Plugin(name) {}
  char const *type() const { return "service"; }
  void start(Core_api *core);

  //Canvas *screen() const { return _ust.vstack()->canvas(); }

  void destroy();
  ~Service();

  long op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                 l4_mword_t proto, L4::Ipc::Varg_list<> const &args);
};

}

