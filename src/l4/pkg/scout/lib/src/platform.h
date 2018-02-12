/*
 * \brief   Platform abstraction
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * This interface specifies the target-platform-specific functions.
 */

/*
 * Copyright (C) 2005-2009
 * Genode Labs, Feske & Helmuth Systementwicklung GbR
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <l4/mag-gfx/geometry>
#include <l4/re/video/goos>
#include <l4/scout-gfx/event>
#include <l4/scout-gfx/window>
#include <l4/scout-gfx/platform>

using Mag_gfx::Rect;
using Mag_gfx::Area;

/*
 * We use two buffers, a foreground buffer that is displayed on screen and a
 * back buffer. While the foreground buffer must contain valid data all the
 * time, the back buffer can be used to prepare pixel data. For example,
 * drawing multiple pixel layers with alpha channel must be done in the back
 * buffer to avoid artifacts on the screen.
 */



class Platform : public Scout_gfx::Platform
{
protected:
  Area _max;

public:
  class Pf_factory
  {
  public:
    static Pf_factory *first;
    Pf_factory *next;
    virtual Platform *create(Rect const &r) = 0;
    Pf_factory() : next(first) { first = this; }
    virtual ~Pf_factory() {}
  };

  template<typename T>
  class Pf_factory_t : public Pf_factory
  {
  public:
    Platform *create(Rect const &r)
    {
      if (T::probe())
	return new T(r);

      return 0;
    }

  };

  static Platform *get(Rect const &r);

  /**
   * Constructor - initialize platform
   *
   * \param vx,vy   initial view position
   * \param vw,vw   initial view width and height
   * \param max_vw  maximum view width
   *
   * When using the default value for 'max_vw', the window's
   * max width will correspond to the screen size.
   */
  Platform(Area const &max = Area(0,0)) : _max(max)
  {}

  /**
   * Check initialization state of the platform
   *
   * \retval 1 platform was successfully initialized
   * \retval 0 platform initialization failed
   */
  virtual int initialized() = 0;

  /**
   * Request screen width and height
   */
  virtual Mag_gfx::Pixel_info const *pixel_info() const = 0;

  virtual ~Platform() {}
};

#endif /* _PLATFORM_H_ */
