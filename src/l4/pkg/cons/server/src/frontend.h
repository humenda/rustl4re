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

#include <l4/cxx/hlist>
#include "input_mux.h"

class Frontend : public cxx::H_list_item
{
public:
  virtual int write(char const *buffer, unsigned size) = 0;
  void input_mux(Input_mux *m) { _input = m; }

  virtual ~Frontend() = 0;

  virtual bool check_input() = 0;

protected:
  Input_mux *_input;
};

inline Frontend::~Frontend() {}
