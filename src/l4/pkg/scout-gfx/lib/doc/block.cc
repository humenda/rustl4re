/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/scout-gfx/doc/block>

namespace Scout_gfx {


/***********
 ** Block **
 ***********/

void
Block::append_text(const char *str, Style *style, Token_factory const &tf)
{
  Token *last = 0;
  while (*str)
    {

      /* skip spaces */
      if (*str == ' ')
	{
	  str++;
	  continue;
	}

      /* search end of word */
      int i;
      for (i = 0; str[i] && (str[i] != ' '); i++)
	;

      /* create and append token for the word */
      if (i)
	{
	  Token *n = tf.create(style, str, i);
	  if (last)
	    last->add_clause(n);

	  last = n;
	  append(n);
	}

      /* continue with next word */
      str += i;
    }
}

Area
Block::min_size() const
{
  int w = 0;
  for (Widget *e = _first; e; e = e->next)
    {
      int ew = e->size().w();
      if (w < ew)
	w = ew;
    }

  return Area(w, 10);
}


int
Block::height_for_width(int w) const
{
  int x = 0, y = 0;
  int line_max_h = 0;

  for (Widget *e = _first; e; e = e->next)
    {
      int ew = e->size().w();
      /* wrap at the end of the line */
      if (x + ew >= w)
	{
	  x  = _second_indent;
	  y += line_max_h;
	  line_max_h = 0;
	}

      /* determine token with the biggest height of the line */
      if (line_max_h < e->size().h())
	line_max_h = e->size().h();

      x += ew;
    }

  /* line break at the end of the last line */
  y += line_max_h;

  return y + 5;
}


void
Block::format_fixed_width(int w)
{
  int x = 0, y = 0;
  int line_max_h = 0;
  int max_w = 0;

  for (Widget *e = _first; e; e = e->next)
    {
      int ew = e->size().w();
      /* wrap at the end of the line */
      if (x + ew >= w)
	{
	  x  = _second_indent;
	  y += line_max_h;
	  line_max_h = 0;
	}

      /* position element */
      if (max_w < x + ew)
	max_w = x + ew;

      e->set_geometry(Rect(Point(x, y), e->size()));

      /* determine token with the biggest height of the line */
      if (line_max_h < e->size().h())
	line_max_h = e->size().h();

      x += ew;
    }

  /*
   * Now, the text is left-aligned.
   * Let's apply another alignment if specified.
   */

  if (_align != LEFT)
    {
      for (Widget *line = _first; line; )
	{
	  Widget *e;
	  int      cy = line->pos().y();   /* y position of current line */
	  int      max_x;            /* rightmost position         */

	  /* determine free space at the end of the line */
	  for (max_x = 0, e = line; e && (e->pos().y() == cy); e = e->next)
	    max_x = std::max(max_x, e->geometry().x2());

	  /* indent elements of the line according to the alignment */
	  int dx = 0;
	  if (_align == CENTER)
	    dx = std::max(0, (w - max_x)/2);

	  if (_align == RIGHT)
	    dx = std::max(0, w - max_x);

	  for (e = line; e && (e->pos().y() == cy); e = e->next)
	    e->set_geometry(e->geometry() + Point(dx, 0));

	  /* find first element of next line */
	  for (; line && (line->pos().y() == cy); line = line->next)
	    ;
	}
    }

  /* line break at the end of the last line */
  y += line_max_h;

  _size = Area(w, y + 5);
}

}
