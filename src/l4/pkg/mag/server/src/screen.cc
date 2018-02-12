/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "factory"

#include <l4/mag-gfx/mem_factory>
#include "mouse_cursor.h"

namespace Mag_server {

using namespace Mag_gfx;


template< typename PT >
class Csf : public Screen_factory, public Mem::Factory<PT>
{
public:

  View *create_cursor(Texture const *cursor)
  {
    typedef typename PT::Pixel Pixel;
    if (cursor->type() == PT::type())
      return new Mouse_cursor<PT>(cursor);
    else
      {
	void *m = malloc(cursor->size().pixels() * sizeof(Pixel));
	Texture *c = new Mem::Texture<PT>((Pixel*)m, cursor->size());
	c->blit(cursor);
	return new Mouse_cursor<PT>(c);
      }
  }

};


static Csf<Rgb15>  _csf_rgb15;
static Csf<Bgr15>  _csf_bgr15;
static Csf<Rgb16>  _csf_rgb16;
static Csf<Bgr16>  _csf_bgr16;
static Csf<Rgb24>  _csf_rgb24;
static Csf<Rgb32>  _csf_rgb32;
static Csf<Bgr32>  _csf_bgr32;
static Csf<Rgba32>  _csf_rgba32;

}
