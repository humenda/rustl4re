/*
 * \brief   PNG image element
 * \date    2005-11-07
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */
/*
 * Copyright (C) 2005-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <png.h>

#include <l4/scout-gfx/png_image>

#include <l4/mag-gfx/canvas>
#include <l4/mag-gfx/factory>
#include <l4/mag-gfx/mem_texture>

using namespace Mag_gfx;
using namespace Scout_gfx;

namespace {
class Png_stream
{
private:

  char *_addr;

public:

  /**
   * Constructor
   */
  Png_stream(char *addr) { _addr = addr; }

  /**
   * Read from png stream
   */
  void read(char *dst, int len)
    {
      memcpy(dst, _addr, len);
      _addr += len;
    }
};


/**
 * PNG read callback
 */
static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t len)
{
  Png_stream *stream = (Png_stream *)png_get_io_ptr(png_ptr);

  stream->read((char *)data, len);
}

}


/***********************
 ** Element interface **
 ***********************/

void Png_image::fill_cache(Mag_gfx::Pixel_info const *c)
{
  if (_texture)
    return;

  Png_stream *stream = new Png_stream((char *)_png_data);

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png_ptr) return;

  png_set_read_fn(png_ptr, stream, user_read_data);

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return;
  }

  png_read_info(png_ptr, info_ptr);

  /* get image data chunk */
  int bit_depth, color_type, interlace_type;
  png_uint_32 w, h;
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
               &interlace_type, NULL, NULL);

  _size = Area(w, h);

  // printf("png[%p] : load -> sz=%d,%d\n", this, w, h);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  if (!(color_type & PNG_COLOR_MASK_ALPHA))
    png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);

  png_set_bgr(png_ptr);

  if (bit_depth < 8) png_set_packing(png_ptr);
  if (bit_depth == 16) png_set_strip_16(png_ptr);

  _texture = c->factory->create_texture(_size, 0, true);

  /* allocate buffer for decoding a row */
  static png_byte *row_ptr;
  static int curr_row_size;

  int needed_row_size = png_get_rowbytes(png_ptr, info_ptr)*8;

  if (curr_row_size < needed_row_size)
    {
      if (row_ptr)
	free(row_ptr);

      row_ptr = (png_byte *)malloc(needed_row_size);
      curr_row_size = needed_row_size;
    }

  Mem::Texture<Rgba32> tt((Rgba32::Pixel *)row_ptr, Area(_size.w(), 1));

  /* fill texture */
  for (int j = 0; j < _size.h(); j++)
    {
      png_read_row(png_ptr, row_ptr, NULL);
      _texture->blit(&tt, j);
    }
  invalidate();
}


void Png_image::flush_cache(Mag_gfx::Pixel_info const *)
{
  delete _texture;
  _texture = 0;
}


void Png_image::draw(Canvas *c, Point const &p)
{
  /* if texture is not ready, try to initialize it */
  if (!_texture)
    {
      fill_cache(c->type());
      refresh();
      return;
    }

  /* draw texture */
  if (_texture)
    c->draw_texture(_texture, Rgb32::Black, p, Canvas::Alpha);
}
