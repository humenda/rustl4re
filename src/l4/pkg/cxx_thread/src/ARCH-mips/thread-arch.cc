/*
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License 2.1.  See the file "COPYING-LGPL-2.1" in the main directory of
 * this archive for more details.
 *
 * Copyright (C) 2013 Imagination Technologies Ltd.
 * Author: Yann Le Du <ledu@kymasys.com>
 */

#include <l4/cxx/thread>

void L4_cxx_start(void);

void L4_cxx_start(void)
{
  /*
   * get _this pointer from the stack and call _this->execute()
   *
   * void Thread::start_cxx_thread(Thread *_this)
   * { _this->execute(); }
   *
   */
  asm volatile (".global L4_Thread_start_cxx_thread \n"
                "L4_Thread_start_cxx_thread:        \n"
                ".set push                          \n"
                ".set noreorder                     \n"
#if (_MIPS_SZLONG == 64)
                "bal 1f                             \n"
                "ld  $a0, 8($sp)                    \n"
                "1:                                 \n"
                ".cpsetup $ra, $t9, 1b              \n"
#else
                ".cprestore 16                      \n"
                "lw  $a0, 4($sp)                    \n"
#endif
                ".set pop                           \n"
                "jal L4_Thread_execute              \n");
}

void L4_cxx_kill(void);

void L4_cxx_kill(void)
{
  /*
   * get _this pointer from the stack and call _this->shutdown()
   *
   * void Thread::kill_cxx_thread(Thread *_this)
   * { _this->shutdown(); }
   *
   */
  asm volatile (".global L4_Thread_kill_cxx_thread \n"
                "L4_Thread_kill_cxx_thread:        \n"
                ".set push                         \n"
                ".set noreorder                    \n"
#if (_MIPS_SZLONG == 64)
                "bal 1f                            \n"
                "ld  $a0, 8($sp)                   \n"
                "1:                                \n"
                ".cpsetup $ra, $t9, 1b             \n"
#else
                ".cprestore 16                     \n"
                "lw  $a0, 4($sp)                   \n"
#endif
                ".set pop                          \n"
                "jal L4_Thread_shutdown            \n");
}

