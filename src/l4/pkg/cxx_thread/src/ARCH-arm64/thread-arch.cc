/*
 * (c) 2004-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

#include <l4/cxx/thread>

void L4_cxx_start(void);

void L4_cxx_start(void)
{
  asm volatile (".global L4_Thread_start_cxx_thread \n"
                "L4_Thread_start_cxx_thread:        \n"
                "ldr x0, [sp, #8]!                  \n"
                "ldr x9, 1f                         \n"
                "br x9                              \n"
                "1: .quad L4_Thread_execute         \n");
}

void L4_cxx_kill(void)
{
  asm volatile (".global L4_Thread_kill_cxx_thread \n"
                "L4_Thread_kill_cxx_thread:        \n"
                "ldr x0, [sp, #8]!                 \n"
                "ldr x9, 1f                        \n"
                "br x9                             \n"
                "1: .quad L4_Thread_shutdown       \n");
}

