/**
 * \file	con/examples/demo3/demo3.c
 * \brief	demonstration server for con
 *
 * pSLIM_CSCS (bird)
 * pSLIM_COPY (`dismembered' bird) */

/* (c) 2003 'Technische Universitaet Dresden'
 * This file is part of the con package, which is distributed under
 * the terms of the GNU General Public License 2. Please see the
 * COPYING file for details. */

/* L4 includes */
#include <l4/thread/thread.h>
#include <l4/sys/kdebug.h>
#include <l4/util/util.h>
#include <l4/util/l4_macros.h>

#include <l4/l4con/l4con.h>
#include <l4/l4con/l4con-client.h>

#include <l4/names/libnames.h>
#include <l4/log/l4log.h>

#define PROGTAG		"_demo3"

#define MY_SBUF_SIZE1	(128*128)
#define MY_SBUF_SIZE2	(128*128/4)
#define MY_SBUF_SIZE3	(128*128/4)

/* OSKit includes */
#include <stdio.h>
#include <stdlib.h>

/* local includes */
#include "util.h"
#include "colors.h"
#include "examples_config.h"
#include "bmaps/cscs.bmap"
#include "bmaps/copy.bmap"
#include "yuvs/bird.yuv"

/* internal prototypes */
int clear_screen(void);
int draw_bird(void);
int dismember_bird(void);

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
int clear_screen()
{
  int ret = 0;
  char buffer[30];

  l4con_pslim_rect_t rect;
  CORBA_Environment _env = dice_default_environment;

  /* setup initial vfb area info */
  rect.x = 0; rect.y = 0;
  rect.w = xres; rect.h = yres;

  ret = con_vc_pslim_fill_call(&vc_l4id, &rect, black, &_env);
  if (ret) 
    {
      ret2ecodestr(ret, buffer);
      printf("pslim_fill returned %s error\n", buffer);
      return -1;
    }

  return 0;
}

/******************************************************************************
 * draw_bird                                                                  *
 *                                                                            *
 * show bird on screen using pSLIM CSCS                                       *
 ******************************************************************************/
int draw_bird()
{
  int ret = 0;
  char buffer[30];
  l4con_pslim_rect_t rect;
  CORBA_Environment _env = dice_default_environment;

  /*** draw upper-right text ***/

  /* setup vfb area info */
  rect.x = xres-84; rect.y = 0;
  rect.w = 88; rect.h = 25;

  ret = con_vc_pslim_bmap_call(&vc_l4id,
			  &rect,
			  black, cornflowerblue,
			  cscs_bmap, 275, 
			  pSLIM_BMAP_START_MSB, &_env);
  if (ret || DICE_HAS_EXCEPTION(&_env))
    {
      ret2ecodestr(ret, buffer);
      printf("Error \"%s\" doing pslim_bmap (exc=%d)\n", buffer, 
	  DICE_EXCEPTION_MAJOR(&_env));
      return -1;
    }

  /*** draw bird ***/

  /* setup initial vfb area info */
  rect.x = (xres-128)/2; rect.y = (yres-128)/2;
  rect.w = 128; rect.h = 128;

  ret = con_vc_pslim_cscs_call(&vc_l4id,
			  &rect, 
			  (l4_uint8_t*)my_yuv, 128*128,
			  (l4_uint8_t*)(my_yuv + 128*128), 128*128/4, 
			  (l4_uint8_t*)(my_yuv + 128*128 + 128*128/4), 128*128/4, 
	  		  pSLIM_CSCS_PLN_YV12,
  			  1, 
			  &_env);

  if (ret || DICE_HAS_EXCEPTION(&_env))
    {
      ret2ecodestr(ret, buffer);
      printf("Error \"%s\" doing pslim_cscs (exc=%d)\n", buffer, 
	  DICE_EXCEPTION_MAJOR(&_env));
      return -1;
    }

  return 0;
}

/******************************************************************************
 * dismember_bird                                                             *
 *                                                                            *
 * copy some screen portion using pSLIM COPY - `exploding bird'               *
 ******************************************************************************/
int dismember_bird()
{
#define inter_copy_delay 20
  int ret = 0, i;
  char buffer[30];
  l4con_pslim_rect_t rect;
  CORBA_Environment _env = dice_default_environment;

  /*** draw upper-right text ***/

  /* setup vfb area info */
  rect.x = xres-84; rect.y = 0;
  rect.w = 88; rect.h = 25;

  ret = con_vc_pslim_bmap_call(&vc_l4id,
			  &rect, black, lightsteelblue,
			  copy_bmap, 275, 
		  	  pSLIM_BMAP_START_MSB, 
	  		  &_env);
  if (ret) 
    {
      ret2ecodestr(ret, buffer);
      printf("pslim_bmap returned %s error\n", buffer);
      return -1;
    }

  /*** `dismemeber' bird ***/
  /* setup initial vfb area info */
  rect.x = xres/2-64; rect.y = yres/2-64;
  rect.w = 64; rect.h = 64;

  for (i=0;i<20;i++) 
    {
      /* upper-left */
      ret = con_vc_pslim_copy_call(&vc_l4id,
			      &rect,
	      		      rect.x - 5,
			      rect.y - 5,
			      &_env);
      /* setup vfb area info */
      rect.x -= 5; rect.y -= 5;
      l4_sleep(inter_copy_delay);
    }
  rect.x = xres/2; rect.y = yres/2-64;
  for (i=0;i<20;i++) 
    {
      /* upper-right */
      ret = con_vc_pslim_copy_call(&vc_l4id,
			      &rect,
	      		      rect.x + 5,
			      rect.y - 5,
			      &_env);
      /* setup vfb area info */
      rect.x += 5; rect.y -= 5;
      l4_sleep(inter_copy_delay);
    }
  rect.x = xres/2; rect.y = yres/2;
  for (i=0;i<20;i++) 
    {
      /* lower-right */
      ret = con_vc_pslim_copy_call(&vc_l4id,
			      &rect,
			      rect.x + 5,
			      rect.y + 5,
	      		      &_env);
      /* setup vfb area info */
      rect.x += 5; rect.y += 5;
      l4_sleep(inter_copy_delay);
    }
  rect.x = xres/2-64; rect.y = yres/2;
  for (i=0;i<20;i++) 
    {
      /* lower-left */
      ret = con_vc_pslim_copy_call(&vc_l4id,
			      &rect,
	      		      rect.x - 5,
			      rect.y + 5,
			      &_env);
      /* setup vfb area info */
      rect.x -= 5; rect.y += 5;
      l4_sleep(inter_copy_delay);
    }
  
  return 0;
}

/******************************************************************************
 * main                                                                       *
 *                                                                            *
 * Main function                                                              *
 ******************************************************************************/
int main(int argc, char *argv[])
{
  int error = 0;
  int i;
  l4_threadid_t dummy_l4id = L4_NIL_ID;

  CORBA_Environment _env = dice_default_environment;
  
  do_args(argc, argv);
  my_l4id = l4thread_l4_id( l4thread_myself() );

  printf("Howdy, I'm "l4util_idfmt"\n", l4util_idstr(my_l4id));

  /* ask for 'con' (timeout = 5000 ms) */
  if (names_waitfor_name(CON_NAMES_STR, &con_l4id, 5000) == 0) 
    {
      printf("PANIC: %s not registered at names", CON_NAMES_STR);
      enter_kdebug("panic");
    }

  if (con_if_openqry_call(&con_l4id, 
		     MY_SBUF_SIZE1, MY_SBUF_SIZE2, MY_SBUF_SIZE3,
		     L4THREAD_DEFAULT_PRIO,
	  	     &vc_l4id, 
  		     CON_VFB,
		     &_env))
    enter_kdebug("Ouch, open vc failed");

  if (con_vc_smode_call(&vc_l4id, CON_OUT, &dummy_l4id, &_env))
    enter_kdebug("Ouch, setup vc failed");

  if (con_vc_graph_gmode_call(&vc_l4id, &gmode, &xres, &yres,
			 &bits_per_pixel, &bytes_per_pixel,
			 &bytes_per_line, &accel_flags, 
			 &fn_x, &fn_y, &_env))
    enter_kdebug("Ouch, graph_gmode failed");

  for (i=0; i<1 && !error; i++)
    {
      if ((error = clear_screen()))
	enter_kdebug("Ouch, clear_screen failed");
      if ((error = draw_bird()))
	enter_kdebug("Ouch, draw_bird failed");
      l4_sleep(200);
      if ((error = dismember_bird()))
	enter_kdebug("Ouch, dismemeber_bird failed");
      l4_sleep(200);
    }

  return 0;
}
