/**
 * \file   l4vfs/term_server/lib/vt100/color.c
 * \brief  color handling
 *
 * \date   08/10/2004
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2004-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
//#include <l4/l4con/l4con_pslim.h>
//#include <l4/log/l4log.h>

#include "lib.h"

// colors should be 0..7, intensity should be 0..2
l4_int8_t pack_attribs(int fg_color, int bg_color, int intensity)
{
    return (fg_color << 3) + bg_color + (intensity << 6);
}

void unpack_attribs(l4_int8_t a, int * fg_color, int * bg_color,
                    int * intensity)
{
    *fg_color = (a >> 3) & 7;
    *bg_color = a & 7;
    *intensity = (a >> 6) & 3;
}
