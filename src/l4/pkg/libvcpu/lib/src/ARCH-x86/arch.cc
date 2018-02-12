/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */

#include <l4/vcpu/vcpu.h>
#include <stdio.h>

void l4vcpu_print_state_arch(l4_vcpu_state_t const *vcpu,
                             const char *prefix) L4_NOTHROW
{
  printf("%sip=%08lx sp=%08lx trapno=%08lx\n",
         prefix, vcpu->r.ip, vcpu->r.sp, vcpu->r.trapno);
  printf("%sax=%08lx dx=%08lx bx=%08lx cx=%08lx\n",
         prefix, vcpu->r.ax, vcpu->r.dx, vcpu->r.bx, vcpu->r.cx);
  printf("%ssi=%08lx di=%08lx bp=%08lx flags=%08lx\n",
         prefix, vcpu->r.si, vcpu->r.di, vcpu->r.bp, vcpu->r.flags);
  printf("%sds=%08lx es=%08lx gs=%08lx fs=%08lx\n",
         prefix, vcpu->r.ds, vcpu->r.es, vcpu->r.gs, vcpu->r.fs);
}
