/**
 * \file	con/examples/demo2/demo2.c
 * \brief	demonstration server for con
 *
 * pSLIM_FILL ('DROPS' - message) */

/* (c) 2003 'Technische Universitaet Dresden'
 * This file is part of the con package, which is distributed under
 * the terms of the GNU General Public License 2. Please see the
 * COPYING file for details. */

/* L4 includes */
#include <l4/thread/thread.h>
#include <l4/sys/kdebug.h>
#include <l4/util/util.h>
#include <l4/util/getopt.h>
#include <l4/util/l4_macros.h>

#include <l4/l4con/l4con.h>
#include <l4/l4con/l4con-client.h>

#include <l4/names/libnames.h>
#include <l4/log/l4log.h>

#define PROGTAG		"_demo2"

#define MY_SBUF_SIZE	32768

/* OSKit includes */
#include <stdio.h>
#include <stdlib.h>

/* local includes */
#include "util.h"
#include "colors.h"
#include "examples_config.h"
#include "demo.h"
#include "bmaps/fill.bmap"

/* internal prototypes */
int draw_pt(int x, int y, int color);

/* global vars */
l4_threadid_t my_l4id;			/* it's me */
l4_threadid_t con_l4id;			/* con at names */
l4_threadid_t vc_l4id;			/* partner VC */
l4_uint8_t gmode;
l4_uint32_t xres, yres;
l4_uint32_t fn_x, fn_y;
l4_uint32_t bits_per_pixel;
l4_uint32_t bytes_per_pixel;
l4_uint32_t bytes_per_line;
l4_uint32_t accel_flags;

/******************************************************************************
 * clear_screen                                                               *
 *                                                                            *
 * Nomen est omen.                                                            *
 ******************************************************************************/
static int
clear_screen(void)
{
  int ret;
  char buffer[30];
  l4con_pslim_rect_t rect;
  l4_strdope_t bmap;
  CORBA_Environment _env = dice_default_environment;

  /* setup initial vfb area info */
  rect.x = 0; rect.y = 0;
  rect.w = xres; rect.h = yres;
  
  ret = con_vc_pslim_fill_call(&vc_l4id, &rect, antiquewhite, &_env);
  if (ret) 
    {
      ret2ecodestr(ret, buffer);
      printf("pslim_fill returned %s error\n", buffer);
      return -1;
    }

  /* setup new vfb area info */
  rect.x = xres-84; rect.y = 0;
  rect.w = 88; rect.h = 25;

  /* setup L4 string dope */
  bmap.snd_size = 275;
  bmap.snd_str = (l4_umword_t) fill_bmap;
  bmap.rcv_size = 0;

  ret = con_vc_pslim_bmap_call(&vc_l4id,
			  &rect, 
			  antiquewhite, indianred,
  			  fill_bmap,
			  275,
			  pSLIM_BMAP_START_MSB, 
			  &_env);
  if (ret) 
    {
      ret2ecodestr(ret, buffer);
      printf("pslim_bmap returned %s error\n", buffer);
      return -1;
    }

  return 0;
}

/******************************************************************************
 * logo                                                                       *
 *                                                                            *
 * show logo on screen using draw_pt                                          *
 ******************************************************************************/
static int
logo(void)
{
#define inter_point_delay 20
  int x, y;
  int off_x = 1, off_y = 1;

  /* D */
  for (y = 0; y < ALPHA_Y; y++)
    for (x = 0; x < ALPHA_X; x++)
      if (alpha_D[y][x]) 
	{
	  if(draw_pt(x+off_x, y+off_y, deepskyblue))
    	    return -1;
	  l4_sleep(inter_point_delay);
	}
  /* R */
  off_x = 9;
  for (y = 0; y < ALPHA_Y; y++)
    for (x = 0; x < ALPHA_X; x++)
      if (alpha_R[y][x]) 
	{
	  if(draw_pt(x+off_x, y+off_y, deepskyblue))
	    return -1;
	  l4_sleep(inter_point_delay);
	}
  /* O */
  off_x = 17;
  for (y = 0; y < ALPHA_Y; y++)
    for (x = 0; x < ALPHA_X; x++)
      if (alpha_O[y][x]) 
	{
	  if(draw_pt(x+off_x, y+off_y, deepskyblue))
	    return -1;
	  l4_sleep(inter_point_delay);
	}
  /* P */
  off_x = 25;
  for (y = 0; y < ALPHA_Y; y++)
    for (x = 0; x < ALPHA_X; x++)
      if (alpha_P[y][x]) 
	{
	  if(draw_pt(x+off_x, y+off_y, deepskyblue))
	    return -1;
	  l4_sleep(inter_point_delay);
	}
  /* S */
  off_x = 33;
  for (y = 0; y < ALPHA_Y; y++)
    for (x = 0; x < ALPHA_X; x++)
      if (alpha_S[y][x]) 
	{
	  if(draw_pt(x+off_x, y+off_y, deepskyblue))
	    return -1;
	  l4_sleep(inter_point_delay);
	}
  
  return 0;
}

/******************************************************************************
 * draw_pt                                                                    *
 *                                                                            *
 * draw `point' using pSLIM FILL                                              *
 ******************************************************************************/
int draw_pt(int x, int y, int color)
{
  int ret = 0;
  char buffer[30];
  l4con_pslim_rect_t rect;
  CORBA_Environment _env = dice_default_environment;

  /* setup initial vfb area info */
  rect.x = x*10; rect.y = y*10;
  rect.w = 9; rect.h = 9;

  ret = con_vc_pslim_fill_call(&vc_l4id, &rect, color, &_env);
  if (ret) 
    {
      ret2ecodestr(ret, buffer);
      printf("pslim_fill returned %s error\n", buffer);
      return -1;
    }

  return 0;
};

/******************************************************************************
 * main                                                                       *
 *                                                                            *
 * Main function                                                              *
 ******************************************************************************/
int main(int argc, char *argv[])
{
  int error = 0;
  l4_threadid_t dummy_l4id = L4_NIL_ID;

  CORBA_Environment _env = dice_default_environment;

  do_args(argc, argv);
  my_l4id = l4thread_l4_id( l4thread_myself() );

  printf("Hello there, I am "l4util_idfmt".\n", l4util_idstr(my_l4id));

  /* ask for 'con' (timeout = 5000 ms) */
  if (names_waitfor_name(CON_NAMES_STR, &con_l4id, 5000) == 0) 
    {
      printf("PANIC: %s not registered at names.\n", CON_NAMES_STR);
      enter_kdebug("panic");
    }

  if (con_if_openqry_call(&con_l4id, MY_SBUF_SIZE, 0, 0, L4THREAD_DEFAULT_PRIO,
		     &vc_l4id, CON_VFB, &_env))
    enter_kdebug("open vc failed");

  if (con_vc_smode_call(&vc_l4id, CON_OUT, &dummy_l4id, &_env))
    enter_kdebug("setup vc failed");

  if (con_vc_graph_gmode_call(&vc_l4id, &gmode, &xres, &yres,
			 &bits_per_pixel, &bytes_per_pixel,
			 &bytes_per_line, &accel_flags, 
			 &fn_x, &fn_y, &_env))
    enter_kdebug("graph_gmode failed");

  while (!error) 
    {
      if ((error = clear_screen()))
	enter_kdebug("clear_screen failed");
      if ((error = logo()))
	enter_kdebug("logo failed");
      
      l4_sleep(2000);
    }

  if (con_vc_close_call(&vc_l4id, &_env))
    enter_kdebug("close vc failed?!");

  printf(PROGTAG "Going to bed ...\n");
  l4_sleep(-1);

  return 0;
}

