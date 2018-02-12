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
#pragma once

L4_CV L4_INLINE
int
l4vcpu_is_irq_entry(l4_vcpu_state_t const *vcpu) L4_NOTHROW
{
  return (vcpu->r.err >> 26) == 0x3f;
}

L4_CV L4_INLINE
int
l4vcpu_is_page_fault_entry(l4_vcpu_state_t const *vcpu) L4_NOTHROW
{
  return ((vcpu->r.err >> 26) & 0x30) == 0x20;
}
