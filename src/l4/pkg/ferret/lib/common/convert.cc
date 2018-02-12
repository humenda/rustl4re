/**
 * \file   ferret/lib/util/convert.c
 * \brief  Convert kernel tracebuffer entries to common header format.
 *
 * \date   09/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <l4/ferret/sensors/tbuf.h>
#include <l4/ferret/util.h>
#include <l4/ferret/types.h>

void ferret_util_convert_k2c(const l4_tracebuffer_entry_t * tb_e,
                             ferret_list_entry_kernel_t * lec)
{
    // common part for list entry
    lec->timestamp = tb_e->tsc;
    lec->major     = FERRET_TBUF_MAJOR;
    lec->minor     = tb_e->type;
    lec->instance  = FERRET_TBUF_INSTANCE;
    lec->cpu       = 0;  // fixme: later

    // common part for all kernel events
    lec->context   = (void *)tb_e->context;  // thread
    lec->eip       = tb_e->eip;
    lec->pmc1      = tb_e->pmc1;
    lec->pmc2      = tb_e->pmc2;

    lec->data8[0]  = 0;  // padding byte

    // kernel event specific parts, directly after padding byte
    memcpy(&lec->data8[1], &tb_e->m, sizeof(tb_e->m.fit._pad));
}
