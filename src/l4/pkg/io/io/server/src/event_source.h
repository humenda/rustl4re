/*
 * (c) 2014 Alexander Warg <alexander.warg@kernkonzept.com>
 *     economic rights: Kerkonzept GmbH (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once


#include <l4/re/event>

namespace Io {

struct Event_source_infos
{
  L4Re::Event_stream_state state;
  L4Re::Event_stream_info info;
};

}


