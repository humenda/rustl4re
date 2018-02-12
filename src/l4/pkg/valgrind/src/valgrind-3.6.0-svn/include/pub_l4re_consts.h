/*--------------------------------------------------------------------*/
/*--- L4Re specific constants                    pub_l4re_consts.h ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2008 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_L4RE_CONSTS_H
#define __PUB_L4RE_CONSTS_H

#define L4RE_KIP_ADDR 0xaffff000
#define L4RE_KERNEL_START 0xb0000000
#define SYSCALL_PAGE 0xeacff000
#define L4RE_UTCB_SIZE 512
#define L4RE_STACKSIZE  (1024*1024*8)

/*
 * Debug options for L4RE
 */
//#define L4RE_DEBUG_EXECUTION

#endif   // __PUB_L4RE_CONSTS_H

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
