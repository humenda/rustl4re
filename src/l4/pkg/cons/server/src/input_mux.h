/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/cxx/string>

class Input_mux
{
public:
  virtual void input(cxx::String const &buf) = 0;
  virtual ~Input_mux() = 0;
};

inline Input_mux::~Input_mux() {}
