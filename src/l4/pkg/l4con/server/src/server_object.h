/*
 * Copyright (C) 2019 Kernkonzept GmbH.
 * Author(s): Marcus Hähnel <marcus.haehnel@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2. Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/cxx/ipc_epiface>

class Server_object
: public L4::Epiface,
  public cxx::H_list_item
{};
