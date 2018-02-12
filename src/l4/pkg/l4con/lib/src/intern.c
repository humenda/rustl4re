/*!
 * \file	con/lib/src/intern.c
 *
 * \brief	intern function of contxt library
 *
 * \author	Mathias Noack <mn3@os.inf.tu-dresden.de>
 *
 */

#include <string.h>
#include "internal.h"

/*****************************************************************************
 *** global variables
 *****************************************************************************/

extern unsigned accel_flags;
int __cvisible = 0;
int bob        = 0;	/* first line of vtc_scrbuf (is a ring buffer */
int fline      = 0;	/* first line of vtc_scrbuf which is displayed */
int sb_x       = 0;	/* x cursor offset into vtc_scrbuf */
int sb_y       = 0;	/* y cursor offset into vtc_scrbuf */
int vtc_cols   = 0;	/* number of screen columns */
int vtc_lines  = 0;	/* number of screen lines */

l4con_pslim_color_t fg_color = 0x00ffffff;
l4con_pslim_color_t bg_color = 0x00000000;

//*****************************************************************************/
/**
 * \brief   Redraw a range of lines.
 * 
 * \param   t              ... top of range
 * \param   b              ... bottom of range
 */
/*****************************************************************************/
static void
_redraw_range(int t, int b)
{
  int y, idx;
  
  for(y=t; y!=b; y++)
    {
      idx = OFS_LINES(fline+y);
      putstocon(0, y, &vtc_scrbuf[idx * vtc_cols], vtc_cols);
    }
  if (__cvisible)
    _cursor(SHOW);
}

//*****************************************************************************/
/**
 * \brief   Add a new line.
 *
 * Add a new line to the screen buffer.
 */
/*****************************************************************************/
static void
_nline(void)
{
  int new_sb_y = OFS_LINES(sb_y+1);
  sb_x = 0;
  
  memset(&vtc_scrbuf[new_sb_y * vtc_cols], ' ', vtc_cols);
  
  if (new_sb_y == bob)
    {
      /* scroll ring buffer one forward */
      if (bob == fline)
	_scroll(0, vtc_lines, SUP, 1);
      bob = OFS_LINES(bob+1);
    }

  sb_y = new_sb_y;
  if (sb_y == OFS_LINES(fline+vtc_lines))
    /* scroll screen one forward */
    _scroll(0, vtc_lines, SUP, 1);
}


//*****************************************************************************/
/**
 * \brief   Copy within the framebuffer.
 * 
 * \param  t              ... top of range
 * \param  b              ... bottom of range
 * \param  dir            ... copy direction
 * \param  lines          ... number of lines 
 *
 */
/*****************************************************************************/
static void
_copy_fb(int t, int b, int dir, int lines)
{
  l4con_pslim_rect_t _rect;
  CORBA_Environment env = dice_default_environment;
  int y;
  
  _rect.x = 0;
  _rect.w = vtc_cols * fn_x;
  _rect.h = BITY(b-t-lines);
  
  if(dir == SDOWN)
    {
      _rect.y = BITY(t);
      y       = BITY(t+lines);
    }
  else
    {
      _rect.y = BITY(t+lines);
      y       = BITY(t);
    }
  
  if (con_vc_pslim_copy_call(&vtc_l4id, &_rect, 0, y, &env))
    LOG("intern.c: pslim_copy failed (exc=%d)", DICE_EXCEPTION_MAJOR(&env));
}


//*****************************************************************************/
/**
 * \brief   Scroll an area of the screen.
 * 
 * \param  t              ... top of range
 * \param  b              ... bottom of range
 * \param  dir            ... scroll direction
 * \param  lines          ... number of lines 
 *
 */
/*****************************************************************************/
void 
_scroll(int t, int b, int dir, int lines)
{
  int gap;
  
  if(t > b)
    return;
   
  switch (dir) 
    {
    case SDOWN:
      gap = OFS_LINES(fline-bob);
  
      if (!lines || !gap)
	return;
  
      if(gap < lines)
	lines = gap;
      
      fline = OFS_LINES(fline-lines);
      
      if ((accel_flags & L4CON_FAST_COPY) && (lines < vtc_lines))
	{
	  _copy_fb(t, b, dir, lines);
	  _redraw_range(t, t+lines);
	}
      else
       	_redraw_range(t, b);
      break;
      
    case SUP:
      gap = OFS_LINES(sb_y-fline);
      
      if ((gap - (vtc_lines - 1)) < lines)
	lines = gap - (vtc_lines - 1);
      
      if (lines <= 0)
	return;
      
      fline = OFS_LINES(fline+lines);
      
      if ((accel_flags & L4CON_FAST_COPY) && (lines < vtc_lines))
	{
	  _copy_fb(t, b, dir, lines);
	  _redraw_range(b-lines, b);
	}
      else
	_redraw_range(t, b);
      break;
    }
}


//*****************************************************************************/
/**
 * \brief   Redraw function.
 */
/*****************************************************************************/
void
_redraw(void)
{
  _redraw_range(0, vtc_lines);
}


//*****************************************************************************/
/**
 * \brief   Cursor function.
 *
 * \param  mode          ... cursor draw mode (SHOW, HIDE)
 */
/*****************************************************************************/
void
_cursor(int mode)
{
  int cursor_y = OFS_LINES(sb_y-fline);
  int fgc = bg_color, bgc = fg_color;
  
  switch(mode)
    {
    case SHOW:
      __cvisible = 1;
      break;
    case HIDE:
      __cvisible = 0;
      fgc = fg_color;
      bgc = bg_color;
      break;
    }
  
  if ((cursor_y >= 0) && (cursor_y < vtc_lines))
    { 
      CORBA_Environment env = dice_default_environment;
      l4_uint8_t cursor[2];
      
      cursor[0] = vtc_scrbuf[sb_y * vtc_cols + sb_x];
      cursor[1] = 0;
      
      con_vc_puts_call(&vtc_l4id, (char*)cursor, 1, 
		       BITX(sb_x), BITY(cursor_y), fgc, bgc, &env);
    }
}


//*****************************************************************************/
/**
 * \brief    Flush the string buffer.
 *
 * \param  s           ... string buffer
 * \param  len         ... length of string buffer
 * \param  __nline     ... new line flag
 *
 * Store the string buffer in the screen buffer and send it to the console.
 */
/*****************************************************************************/
void
_flush(l4_uint8_t *s, int len, int __nline)
{
  char *d;
  int l;

  if (!__init)
    return;

  if(len > 0)
    {
      int cursor_y = OFS_LINES(sb_y-fline);

      for (l=len, d=&vtc_scrbuf[sb_y * vtc_cols + sb_x]; l; l--)
	*d++ = *s++;

      if (cursor_y >= 0 && cursor_y < vtc_lines)
	putstocon(sb_x, cursor_y, &vtc_scrbuf[sb_y*vtc_cols + sb_x], len);
      /* else not visible */
    }
  
  if(__nline) 
    _nline();
  else
    sb_x += len;
}

