/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/capability>

namespace Ferret {
struct Client : L4::Kobject_t<Client, L4::Kobject, 0>
{
  enum ClientOperations
  {
    Create = 0x10,
    Free   = 0x20,
    NewInstance = 0x30,
  };
};

struct Monitor : L4::Kobject_t<Monitor, L4::Kobject, 1>
{
  enum MonitorOperations
  {
    Attach = 0x10,
    Detach = 0x20,
    List   = 0x30,
  };
};

struct Ding : L4::Kobject_2t<Ding, Client, Monitor> {};

}
