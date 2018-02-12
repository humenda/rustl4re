/*
 * (c) 2012-2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "frontend.h"
#include "client.h"
#include "input_mux.h"
#include "output_mux.h"

class Mux : public Input_mux, public Output_mux
{
public:
  virtual void add_frontend(Frontend *) = 0;
};
