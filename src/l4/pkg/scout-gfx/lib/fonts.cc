/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/scout-gfx/fonts>

extern char _binary_mono16_tff_start[];
extern char _binary_verabi10_tff_start[];
extern char _binary_vera16_tff_start[];
extern char _binary_verai16_tff_start[];
extern char _binary_vera18_tff_start[];
extern char _binary_vera20_tff_start[];
extern char _binary_vera24_tff_start[];

namespace Scout_gfx { namespace Fonts {

Mag_gfx::Font verabi10 (&_binary_verabi10_tff_start[0]);
Mag_gfx::Font vera16   (&_binary_vera16_tff_start[0]);
Mag_gfx::Font verai16  (&_binary_verai16_tff_start[0]);
Mag_gfx::Font mono16   (&_binary_mono16_tff_start[0]);
Mag_gfx::Font vera24   (&_binary_vera24_tff_start[0]);
Mag_gfx::Font vera20   (&_binary_vera20_tff_start[0]);
Mag_gfx::Font vera18   (&_binary_vera18_tff_start[0]);

}}
