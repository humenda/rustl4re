/**
 * \file   ferret/lib/local_names/local_names.c
 * \brief  Manage local names for kernel objects (e.g., Fiasco's User
 *         Semaphores)
 *
 * \date   2007-06-20
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>

#include <l4/l4rm/l4rm.h>
#include <l4/log/l4log.h>
#include <l4/sys/l4int.h>
#include <l4/util/bitops.h>

#include <l4/ferret/types.h>
#include <l4/ferret/local_names.h>

#define MAX_LOCAL_NAMES 256  // fixme: the kernel should export this constant

static l4_mword_t
bitarray[(MAX_LOCAL_NAMES + L4_MWORD_BITS - 1) / L4_MWORD_BITS];

int ferret_local_names_reserve(void)
{
    int i;
    for (i = 1; i < MAX_LOCAL_NAMES; i++)
    {
        if (! l4util_bts(i % L4_MWORD_BITS, &(bitarray[i / L4_MWORD_BITS])))
        {
            break;
        }
    }
    if (i >= MAX_LOCAL_NAMES)
        return 0;
    else
        return i;
}

void ferret_local_names_dispose(int index)
{
    if (index < MAX_LOCAL_NAMES)
    {
        l4util_clear_bit(index % L4_MWORD_BITS,
                         &(bitarray[index / L4_MWORD_BITS]));
    }
}
