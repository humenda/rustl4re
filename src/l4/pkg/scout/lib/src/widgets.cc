/*
 * \brief   GUI elements
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "widgets.h"


using Mag_gfx::Rgba32;
using Mag_gfx::color_conv;

/***********************
 ** Horizontal shadow **
 ***********************/

template <typename PT, int INTENSITY>
void Horizontal_shadow<PT, INTENSITY>::draw(Canvas *c, Point const &_p)
{
  typedef typename PT::Pixel Pixel;
  typedef typename PT::Color Color;

  char *addr = (char *)c->buffer();

  if (!addr)
    return;

  Rect clip = c->clip();
  Rect cr = (_pos + _p) & clip;

  if (!cr.valid())
    return;

  int curr_a = INTENSITY;
  int step   = _pos.h() ? (curr_a/_pos.h()) : 0;

  int bpl = c->bytes_per_line();

  addr += bpl * cr.y1() + cr.x1() * sizeof(Pixel);

  Color shadow_color(0,0,0);

  int h = cr.h();
  for (int j = 0; j < h; j++, addr += bpl)
    {
      Pixel *d = reinterpret_cast<Pixel*>(addr);
      int w = cr.w();

      for (int i = 0; i < w; i++, d++)
	*d = PT::mix(*d, shadow_color, curr_a);

      curr_a -= step;
    }
}


/**********
 ** Icon **
 **********/

template <typename PT, int W, int H>
Icon<PT, W, H>::Icon()
{
  memset(_pixel,  0, sizeof(_pixel));
  memset(_alpha,  0, sizeof(_alpha));
  memset(_shadow, 0, sizeof(_shadow));
  _icon_alpha = 255;
}


template <typename PT, int W, int H>
void Icon<PT, W, H>::rgba(void const *_src, int vshift, int shadow)
{
  typedef typename PT::Pixel Pixel;
  typedef typename PT::Color Color;
  
  Rgba32::Pixel const *src = reinterpret_cast<Rgba32::Pixel const *>(_src);
  /* convert rgba values to pixel type and alpha channel */
  for (int j = 0; j < H; j++)
    for (int i = 0; i < W; i++, ++src)
      {
	Rgba32::Color s = *src;
	_pixel[j][i] = color_conv<Color>(s);
	_alpha[j][i] = s.a();
      }

  /* handle special case of no shadow */
  if (shadow == 0) return;

  /* generate shadow shape from blurred alpha channel */
  for (int j = 1; j < H - 4; j++)
    for (int i = 1; i < W - 2; i++) {
	int v = 0;
	for (int k = -1; k <= 1; k++)
	  for (int l = -1; l <=1; l++)
	    v += _alpha[(j + k + H)%H][(i + l + W)%W];

	_shadow[j + 3][i] = v>>shadow;
    }

  /* shift vertically */
  if (vshift > 0)
    for (int j = H - 1; j >= vshift; j--)
      for (int i = 0; i < W; i++) {
	  _pixel[j][i] = _pixel[j - vshift][i];
	  _alpha[j][i] = _alpha[j - vshift][i];
      }

  /* apply shadow to pixels */
  Color shcol(0, 0, 0);
  for (int j = 0; j < H; j++)
    for (int i = 0; i < W; i++) {
	_pixel[j][i] = PT::mix(shcol, _pixel[j][i], _alpha[j][i]);
	_alpha[j][i] = std::min(255, _alpha[j][i] + _shadow[j][i]);
    }
}


static inline void blur(unsigned char *src, unsigned char *dst, int w, int h)
{
	const int kernel = 3;
	int scale  = (kernel*2 + 1)*(kernel*2 + 1);

	scale = (scale*210)>>8;
	for (int j = kernel; j < h - kernel; j++)
		for (int i = kernel; i < w - kernel; i++) {
			int v = 0;
			for (int k = -kernel; k <= kernel; k++)
				for (int l = -kernel; l <= kernel; l++)
					v += src[w*(j + k) + (i + l)];

			dst[w*j + i] = min(v/scale, 255);
		}
}


template <typename PT, int W, int H>
void Icon<PT, W, H>::glow(unsigned char const *_src, Color c)
{
  typedef typename PT::Color MColor;
  Rgba32::Pixel const *src = reinterpret_cast<Rgba32::Pixel const*>(_src);
  /* extract shape from alpha channel of rgba source image */
  for (int j = 0; j < H; j++)
    for (int i = 0; i < W; i++, ++src)
      _alpha[j][i] = Rgba32::Color(*src).a();

  for (int i = 0; i < 2; i++)
    {
      blur(_alpha[0], _shadow[0], W, H);
      blur(_shadow[0], _alpha[0], W, H);
    }

  /* assign pixels and alpha */
  MColor s = color_conv<MColor>(c);
  for (int j = 0; j < H; j++)
    for (int i = 0; i < W; i++)
      _pixel[j][i] = s;
}


/*
 * An Icon has the following layout:
 *
 *  P1---+--------+----+
 *  | cs |   hs   | cs |   top row
 *  +----P2-------+----+
 *  |    |        |    |
 *  | vs |        | vs |   mid row
 *  |    |        |    |
 *  +----+--------P3---+
 *  | cs |   hs   | cs |   low row
 *  +------------------P4
 *
 * cs ... corner slice
 * hs ... horizontal slice
 * vs ... vertical slice
 */


/**
 * Copy pixel with alpha
 */
template <typename PT>
static inline void transfer_pixel(PT const *src, int src_a, int alpha, PT *dst)
{
  if (src_a)
    {
      int register a = (src_a * alpha)>>8;
      if (a) *dst = PT::Traits::mix(*dst, *src, a);
    }
}


/**
 * Draw corner slice
 */
template <typename PT>
static void draw_cslice(PT const *src, unsigned char const *src_a,
                        int src_pitch, int alpha,
                        char *dst, int dst_pitch, int w, int h)
{
  for (int j = 0; j < h; j++)
    {

      PT const      *s  = src;
      unsigned char const *sa = src_a;
      PT            *d  = reinterpret_cast<PT*>(dst);

      for (int i = 0; i < w; i++, s++, sa++, d++)
	transfer_pixel(s, *sa, alpha, d);

      src += src_pitch;
      src_a += src_pitch;
      dst += dst_pitch;
    }
}


/**
 * Draw horizontal slice
 */
template <typename PT>
static void draw_hslice(PT const *src, unsigned char const *src_a,
                        int src_pitch, int alpha,
                        char *dst, int dst_pitch, int w, int h)
{
  for (int j = 0; j < h; j++)
    {
      PT const *s = src;
      int sa = *src_a;
      PT  *d =  reinterpret_cast<PT*>(dst);

      for (int i = 0; i < w; i++, d++)
	transfer_pixel(s, sa, alpha, d);

      src += src_pitch;
      src_a += src_pitch;
      dst += dst_pitch;
    }
}


/**
	for (int j = 0; j < h; j++) {

		PT   s = *src;
		int sa = *src_a;
		PT  *d =  dst;

		for (int i = 0; i < w; i++, d++)
			transfer_pixel(s, sa, alpha, d);

		src += src_pitch, src_a += src_pitch, dst += dst_pitch;
	}
 * Draw vertical slice
 */
template <typename PT>
static void draw_vslice(PT const *src, unsigned char const *src_a,
                        int, int alpha,
                        char *dst, int dst_pitch, int w, int h)
{
  for (int i = 0; i < w; i++)
    {

      PT const *s = src;
      int sa = *src_a;
      char *d =  dst;

      for (int j = 0; j < h; j++, d += dst_pitch)
	transfer_pixel(s, sa, alpha, reinterpret_cast<PT*>(d));

      src += 1;
      src_a += 1;
      dst += sizeof(PT);
    }
}


/**
 * Draw center slice
 */
template <typename PT>
static void draw_center(PT const *src, unsigned char const *src_a,
                        int, int alpha,
                        char *dst, int dst_pitch, int w, int h)
{
  PT const *s = src;
  int sa = *src_a;

  for (int j = 0; j < h; j++, dst += dst_pitch)
    {

      PT  *d =  reinterpret_cast<PT*>(dst);

      for (int i = 0; i < w; i++, d++)
	transfer_pixel(s, sa, alpha, d);
    }
}


/**
 * Clip rectangle against clipping region
 *
 * The out parameters are the resulting x/y offsets and the
 * visible width and height.
 *
 * \return  1 if rectangle intersects with clipping region,
 *          0 otherwise
 */
static inline int clip(int px1, int py1, int px2, int py2,
                       Rect const &c,
                       int *out_x, int *out_y, int *out_w, int *out_h)
{
  /* determine intersection of rectangle and clipping region */
  int x1 = std::max(px1, c.x1());
  int y1 = std::max(py1, c.y1());
  int x2 = std::min(px2, c.x2());
  int y2 = std::min(py2, c.y2());

  *out_w = x2 - x1 + 1;
  *out_h = y2 - y1 + 1;
  *out_x = x1 - px1;
  *out_y = y1 - py1;

  return (*out_w > 0) && (*out_h > 0);
}


template <typename PT, int W, int H>
void Icon<PT, W, H>::draw(Canvas *c, Point const &p)
{
  typedef typename PT::Pixel Pixel;
  typedef typename PT::Color Color;

  char *addr = (char *)c->buffer();

  if (!addr || (_icon_alpha == 0)) return;

  Rect const cl = c->clip();
  int const bpl = c->bytes_per_line();

  /* determine point positions */
  const int x1 = p.x() + _pos.x1();
  const int y1 = p.y() + _pos.y1();
  const int x4 = x1 + _pos.w() - 1;
  const int y4 = y1 + _pos.h() - 1;
  const int x2 = x1 + W/2;
  const int y2 = y1 + H/2;
  const int x3 = std::max(x4 - W/2, x2);
  const int y3 = std::max(y4 - H/2, y2);

  const int tx1 = 0;
  const int ty1 = 0;
  const int tx4 = W - 1;
  const int ty4 = H - 1;
  const int tx2 = W/2;
  const int ty2 = H/2;
  const int tx3 = std::max(tx4 - W/2, tx2);
  const int ty3 = std::max(ty4 - H/2, ty2);

  Pixel const *src   = _pixel[0] + W*ty1;
  unsigned char const *src_a = _alpha[0] + W*ty1;
  int dx, dy, w, h;

  /*
   * top row
   */

  if (clip(x1, y1, x2 - 1, y2 - 1, cl, &dx, &dy, &w, &h))
    draw_cslice(src + tx1 + dy*W + dx, src_a + tx1 + dy*W + dx, W, _icon_alpha,
	addr + (y1 + dy)*bpl + (x1 + dx) * sizeof(Pixel), bpl, w, h);

  if (clip(x2, y1, x3 - 1, y2 - 1, cl, &dx, &dy, &w, &h))
    draw_hslice(src + tx2 + dy*W + dx, src_a + tx2 + dy*W + dx, W, _icon_alpha,
	addr + (y1 + dy)*bpl + (x2 + dx) * sizeof(Pixel), bpl, w, h);

  if (clip(x3, y1, x4, y2 - 1, cl, &dx, &dy, &w, &h))
    draw_cslice(src + tx3 + dy*W + dx, src_a + tx3 + dy*W + dx, W, _icon_alpha,
	addr + (y1 + dy)*bpl + (x3 + dx) * sizeof(Pixel), bpl, w, h);

  /*
   * mid row
   */

  src   = _pixel[0] + W*ty2;
  src_a = _alpha[0] + W*ty2;

  if (clip(x1, y2, x2 - 1, y3 - 1, cl, &dx, &dy, &w, &h))
    draw_vslice(src + tx1 + dx, src_a + tx1 + dx, W, _icon_alpha,
	addr + (y2 + dy)*bpl + (x1 + dx) * sizeof(Pixel), bpl, w, h);

  if (clip(x2, y2, x3 - 1, y3 - 1, cl, &dx, &dy, &w, &h))
    draw_center(src + tx2, src_a + tx2, W, _icon_alpha,
	addr + (y2 + dy)*bpl + (x2 + dx) * sizeof(Pixel), bpl, w, h);

  if (clip(x3, y2, x4, y3 - 1, cl, &dx, &dy, &w, &h))
    draw_vslice(src + tx3 + dx, src_a + tx3 + dx, W, _icon_alpha,
	addr + (y2 + dy)*bpl + (x3 + dx) * sizeof(Pixel), bpl, w, h);

  /*
   * low row
   */

  src   = _pixel[0] + W*ty3;
  src_a = _alpha[0] + W*ty3;

  if (clip(x1, y3, x2 - 1, y4, cl, &dx, &dy, &w, &h))
    draw_cslice(src + tx1 + dy*W + dx, src_a + tx1 + dy*W + dx, W, _icon_alpha,
	addr + (y3 + dy)*bpl + (x1 + dx) * sizeof(Pixel), bpl, w, h);

  if (clip(x2, y3, x3 - 1, y4, cl, &dx, &dy, &w, &h))
    draw_hslice(src + tx2 + dy*W + dx, src_a + tx2 + dy*W + dx, W, _icon_alpha,
	addr + (y3 + dy)*bpl + (x2 + dx) * sizeof(Pixel), bpl, w, h);

  if (clip(x3, y3, x4, y4, cl, &dx, &dy, &w, &h))
    draw_cslice(src + tx3 + dy*W + dx, src_a + tx3 + dy*W + dx, W, _icon_alpha,
	addr + (y3 + dy)*bpl + (x3 + dx) * sizeof(Pixel), bpl, w, h);
}


template <typename PT, int W, int H>
Element *Icon<PT, W, H>::find(Point const &p)
{
  if (!Element::find(p))
    return 0;

  Point n = p - _pos.p1();

  /* check icon boundaries (the height is flexible) */
  if (!Rect(Point(0,0), Area(W, _pos.h())).contains(n))
    return 0;

  /* upper part of the icon */
  if (n.y() <= H/2)
    return _alpha[n.y()][n.x()] ? this : 0;

  /* lower part of the icon */
  if (n.y() > _pos.h() - H/2)
    return _alpha[n.y() - _pos.h() + H][n.x()] ? this : 0;

  /* middle part of the icon */
  if (_alpha[H/2][n.x()])
    return this;

  return 0;
}


