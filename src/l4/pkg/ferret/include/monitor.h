/**
 * \file   ferret/include/monitor.h
 * \brief  Interface for client applications to sensor directory
 *
 * \date   07/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 * \author Bjoern Doebel   <doebel@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_MONITOR_H_
#define __FERRET_INCLUDE_MONITOR_H_

#include <l4/ferret/types.h>
#include <l4/ferret/monitor_list.h>

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

// macro to hide the casting to void ** of addr
#define ferret_att(srv, major, minor, instance, addr)                  \
    ferret_attach((srv), (major), (minor), (instance), (void **)&(addr))

L4_CV int ferret_attach(l4_cap_idx_t srv,
                        uint16_t major, uint16_t minor,
                        uint16_t instance, void ** addr);
L4_CV int ferret_detach(l4_cap_idx_t srv,
                        uint16_t major, uint16_t minor,
                        uint16_t instance, void ** addr);
L4_CV int ferret_list(l4_cap_idx_t srv,
                      ferret_monitor_list_entry_t ** entries, int count,
                      int offset);

EXTERN_C_END

#endif
