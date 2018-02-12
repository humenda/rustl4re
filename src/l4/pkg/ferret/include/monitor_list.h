/**
 * \file   ferret/include/monitor_list.h
 * \brief  Transfer type for IDL to directory
 *
 * \date   15/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_MONITOR_LIST_H_
#define __FERRET_INCLUDE_MONITOR_LIST_H_

#include <l4/ferret/types.h>

typedef struct
{
    uint16_t major;
    uint16_t minor;
    uint16_t instance;
    uint16_t type;
    uint32_t id;
} ferret_monitor_list_entry_t;

#endif
