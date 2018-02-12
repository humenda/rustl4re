/*
 * (c) 2004-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/cxx/thread>

namespace cxx {

  void Thread::start_cxx_thread(Thread *_this)
  { _this->execute(); }

  void Thread::kill_cxx_thread(Thread *_this)
  { _this->shutdown(); }

};

