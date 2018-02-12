/*
 * (c) 2004-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/cxx/thread>

namespace cxx {
void L4_cxx_start(void)
{
  asm volatile (".global L4_Thread_start_cxx_thread \n"
                "L4_Thread_start_cxx_thread:        \n"
                "mov 8(%rsp), %rdi                  \n"
                "call L4_Thread_execute             \n");
}

  void Thread::kill_cxx_thread(Thread *_this)
  { _this->shutdown(); }

};

