/*
 * \brief  Generic blitting function
 * \author Norman Feske
 * \date   2007-10-10
 */
/*
 * (c) 2007 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/mag-gfx/blit>
#include <cstring>


namespace Mag_gfx { namespace Blit {

void blit(void const *s, unsigned src_w,
          void *d, unsigned dst_w,
          int w, int h)
{
	char const *src = (char const *)s;
	char *dst = (char *)d;

	for (int i = h; i--; src += src_w, dst += dst_w)
		memcpy(dst, src, w);
}

}}
