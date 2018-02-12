/**
 * \file
 * \brief   Shared header file
 *
 * \date
 * \author  Adam Lackorzynski <adam@os.inf.tu-dresden.de> */

/*
 * (c) 2014
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <sys/cdefs.h>
#include <l4/sys/l4int.h>
#include <l4/cxx/hlist>

struct Rtc : cxx::H_list_item_t<Rtc>
{
  virtual int set_time(l4_uint64_t nsec_offset_1970) = 0;
  virtual int get_time(l4_uint64_t *nsec_offset_1970) = 0;
  virtual bool probe() = 0;
  virtual ~Rtc() = 0;

  static Rtc* find_rtc()
  {
    for (auto o = _rtcs.begin(); o != _rtcs.end(); ++o)
      {
        if (o->probe())
          return *o;
      }

    return 0;
  }

  Rtc() { _rtcs.add(this); }

private:
  static cxx::H_list_t<Rtc> _rtcs;
};

inline Rtc::~Rtc() {}
