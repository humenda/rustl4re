/*
 * \brief   Interface of refracted icon
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

#ifndef _REFRACTED_ICON_H_
#define _REFRACTED_ICON_H_

#include <l4/scout-gfx/icon>
#include <l4/scout-gfx/widget>

/**
 * \param PT  pixel type (must be Pixel_rgba compatible)
 * \param DT  distortion map entry type
 */
template <typename PT, typename DT>
class Refracted_icon : public Scout_gfx::Widget
{
private:
  typedef typename PT::Pixel Pixel;

  Pixel         *_backbuf;                 /* pixel back buffer       */
  int            _filter_backbuf;          /* backbuf filtering flag  */
  DT            *_distmap;                 /* distortion table        */
  int            _distmap_w, _distmap_h;   /* size of distmap         */
  Pixel         *_fg;                      /* foreground pixels       */
  unsigned char *_fg_alpha;                /* foreground alpha values */
  Scout_gfx::Orientations _expanding;

public:
  Area _min_size;
  ~Refracted_icon() {}

  Scout_gfx::Area min_size() const { return _min_size; }
  Scout_gfx::Area preferred_size() const { return _size; }
  Scout_gfx::Area max_size() const { return Scout_gfx::Area(100000,100000); }
  bool empty() const { return false; }
  Scout_gfx::Orientations expanding() const { return _expanding; }
  void set_expanding(Scout_gfx::Orientations e) { _expanding = e; }
  Scout_gfx::Rect geometry() const { return Scout_gfx::Rect(_pos, _size); }
  void set_geometry(Scout_gfx::Rect const &s) { _pos = s.p1(); size(s.area()); }
  /**
   * Define pixel back buffer for the icon.  This buffer is used for the
   * draw operation.  It should have the same number of pixels as the
   * distortion map.
   */
  void backbuf(Pixel *backbuf, int filter_backbuf = 0)
  {
    _backbuf        = backbuf;
    _filter_backbuf = filter_backbuf;
  }


  void size(Mag_gfx::Area const &a) { _size = a; }

  /**
   * Scratch refraction map
   */
  void scratch(int jitter);

  /**
   * Define distortion map for the icon
   */
  void distmap(DT *distmap, int distmap_w, int distmap_h)
  {
    _distmap   = distmap;
    _distmap_w = distmap_w;
    _distmap_h = distmap_h;
    _size = Area(distmap_w/2, distmap_h/2);
  }

  /**
   * Define foreground pixels
   */
  void foreground(Pixel *fg, unsigned char *fg_alpha)
  {
    _fg       = fg;
    _fg_alpha = fg_alpha;
  }

  void draw(Mag_gfx::Canvas *c, Point const &p);
};

#include "refracted_icon.cc"

#endif /* _REFRACTED_H_ */
