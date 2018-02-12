/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/redraw_manager>
#include <l4/mag-gfx/canvas>

void
Scout_gfx::Redraw_manager::request(Rect const &r)
{
  Rect u = r;
  /*
   * Quick fix to avoid artifacts at the icon bar.
   * The icon bar must always be drawn completely
   * because of the interaction of the different
   * layers.
   */
  if (u.y1() < 64 + 32)
    u = Rect(Point(0,0), Area(_win.w(), std::max(u.h() + u.y1(), 64 + 32)));

  /* first request since last process operation */
  if (_cnt == 0)
    _dirty = u;
  else
    /* merge subsequencing requests */
    _dirty |= u;

  _cnt++;
}

/**
 * Process redrawing operations
 */
void
Scout_gfx::Redraw_manager::process(Rect const &view)
{
  if (_cnt == 0 || !_canvas || !_root)
    return;

  /* get actual drawing area (clipped against canvas dimensions) */
  _cnt = 0;
  Rect d = /*Rect(Point(0, 0), _canvas->size()) &*/ _dirty;

  if (!d.valid())
    return;

  _canvas->set_clipping(d);

  /* draw browser window into back buffer */
  _root->try_draw(_canvas, Point(0, 0));

  /*
   * If we draw the whole area, we can flip the front
   * and back buffers instead of copying pixels from the
   * back to the front buffer.
   */

  /* determine if the whole area must be drawn */
  if (d == view)
    {
      /* flip back end front buffers */
      _scr_update->flip_buf_scr();
    }
  else
    _scr_update->copy_buf_to_scr(d);

  /* give notification about changed canvas area */
  if (_scr_update)
    _scr_update->scr_update(d);

  /* reset request state */
}
