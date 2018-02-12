
/*	con/examples/exit/exit.c
 *
 *	this deom is part of the exit-handling-framework
 *	and originally derived from from con_demo1
 *
 *	pSLIM_SET ('DROPS CON 2000' - logo)
 */

/* L4 includes */
#include <l4/thread/thread.h>
#include <l4/sys/kdebug.h>
#include <l4/util/util.h>
#include <l4/util/l4_macros.h>

#include <l4/l4con/l4con.h>
#include <l4/l4con/l4con-client.h>

#include <l4/names/libnames.h>
#include <l4/log/l4log.h>
#include <l4/events/events.h>
#include <l4/loader/loader-client.h>

#define PROGTAG		"con_exit"

#define MY_SBUF_SIZE	65536

/* OSKit includes */
#include <stdio.h>
#include <stdlib.h>

/* local includes */
#include "util.h"
#include "colors.h"
#include "examples_config.h"
#include "bmaps/set.bmap"
#include "pmaps/drops.pmap"

char LOG_tag[9] = PROGTAG;
l4_ssize_t l4libc_heapsize = DEMO1_MALLOC_MAX_SIZE;

/* internal prototypes */
int clear_screen(void);
int create_logo(void);
int logo(void);

/* global vars */
l4_threadid_t my_l4id;			/* it's me */
l4_threadid_t con_l4id;			/* con at names */
l4_threadid_t vc_l4id;			/* partner VC */
l4_uint16_t *pmap_buf;			/* logo buffer pointer */
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
      LOG("pslim_fill returned %s error", buffer);
      return -1;
    }
  if (DICE_HAS_EXCEPTION(&_env))
    {
      LOG("pslim_fill returned IPC error: 0x%02x", DICE_IPC_ERROR(&_env));
      CORBA_exception_free(&_env);
    }

  /* setup new vfb area info */
  rect.x = xres-84; rect.y = 0;
  rect.w = 88; rect.h = 25;

  ret = con_vc_pslim_bmap_call(&vc_l4id,
			  &rect, 
		  	  black, white, set_bmap, 275, 
			  pSLIM_BMAP_START_MSB, 
			  &_env);
  if (ret) 
    {
      ret2ecodestr(ret, buffer);
      LOG("pslim_bmap returned %s error", buffer);
      return -1;
    }
  if (DICE_HAS_EXCEPTION(&_env))
    {
      LOG("pslim_bmap returned IPC error: 0x%02x", DICE_IPC_ERROR(&_env));
      CORBA_exception_free(&_env);
    }
  
  return 0;
}

/******************************************************************************
 * create_logo                                                                *
 *                                                                            *
 * 24 bit picture to 16 bit pmap                                              *
 ******************************************************************************/
int create_logo()
{
  int i;

  /* create 16 bit picture from logo_pmap */
  if (!(pmap_buf = malloc(160*120*2)))
    return 1;

  for (i=0; i<160*120; i++) 
    {
      pmap_buf[i] = set_rgb16(logo_pmap[3*i], 
			      logo_pmap[3*i+1], 
			      logo_pmap[3*i+2]);
    }
  
  return 0;
}

/******************************************************************************
 * logo                                                                       *
 *                                                                            *
 * show logo on screen using pSLIM SET                                        *
 ******************************************************************************/
int logo()
{
  int ret = 0, i;
  char buffer[30];

  static l4_int16_t xy_idx[][2] = 
    {
	{ 170,  10}, { 300, 100}, 
	{   0, 360}, { 470, 280},
	{ -50, 200}, { 280, 290},
	{ 540, 400}, { 300, 300},
	{ 330, 350}, { 400,-100}
    };

  l4con_pslim_rect_t rect;
  CORBA_Environment _env = dice_default_environment;

  /* setup initial vfb area info */
  rect.x = 0; rect.y = 0;
  rect.w = 160; rect.h = 120;

  for (i=0; i<=0; i++) 
    {
      ret = con_vc_pslim_set_call(&vc_l4id, 
			     &rect,
	      		     (l4_uint8_t*)pmap_buf, 
			     38400,
			     &_env);
      if (ret) 
	{
	  ret2ecodestr(ret, buffer);
	  LOG("pslim_set returned %s error", buffer);
	  return -1;
	}
      if (DICE_HAS_EXCEPTION(&_env))
	{
	  LOG("pslim_set returned ipc error: 0x%02x", DICE_IPC_ERROR(&_env));
	  CORBA_exception_free(&_env);
	}

      /* setup new vfb area info */
      rect.x = xy_idx[i][0]; rect.y = xy_idx[i][1];

      //l4_sleep(1);
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
  int error = 0, i=1;
  l4_threadid_t dummy_l4id = L4_NIL_ID, loader_id;
//  l4events_event_t event;
//  l4events_nr_t eventnr;

  CORBA_Environment _env = dice_default_environment;

  /* init */
  do_args(argc, argv);
  my_l4id = l4thread_l4_id( l4thread_myself() );

  LOG("Hello, I'm running as "l4util_idfmt, l4util_idstr(my_l4id));

  /* ask for 'con' (timeout = 5000 ms) */
  if (names_waitfor_name(CON_NAMES_STR, &con_l4id, 50000) == 0) 
    {
      LOG("PANIC: %s not registered at names", CON_NAMES_STR);
      enter_kdebug("panic");
    }

  if (names_waitfor_name("LOADER", &loader_id, 50000) == 0)
    {
      LOG("PANIC: LOADER not registered at names");
      enter_kdebug("panic");
    }
  
  if (con_if_openqry_call(&con_l4id, MY_SBUF_SIZE, 0, 0,
		     L4THREAD_DEFAULT_PRIO,
		     &vc_l4id, 
	  	     CON_VFB, &_env))
    enter_kdebug("Ouch, open vc failed");
  
  if (con_vc_smode_call(&vc_l4id, CON_OUT, &dummy_l4id, &_env))
    enter_kdebug("Ouch, setup vc failed");

  if (con_vc_graph_gmode_call(&vc_l4id, &gmode, &xres, &yres,
			 &bits_per_pixel, &bytes_per_pixel,
			 &bytes_per_line, &accel_flags, 
			 &fn_x, &fn_y, &_env))
    enter_kdebug("Ouch, graph_gmode failed");

  if (bytes_per_pixel != 2)
    {
      printf("Graphics mode not 2 bytes/pixel, exiting\n");
      con_vc_close_call(&vc_l4id, &_env);
      exit(0);
    }

  if (create_logo())
    enter_kdebug("Ouch, logo creation failed");

  while (!error && (i>0)) 
    {
      if ((error = clear_screen()))
	enter_kdebug("Ouch, clear_screen failed");
      if ((error = logo()))
	enter_kdebug("Ouch, logo failed");
      l4_sleep(500);
      i--;
    }

  if (con_vc_close_call(&vc_l4id, &_env))
    enter_kdebug("Ouch, close vc failed?!");
  
  LOG("Finally closed vc");

  LOG("Going to bed ...");

  names_register("CON_DEMO1");
/*
  my_id = l4_myself();
  event.len=sizeof(l4_umword_t);
  *(l4_umword_t*)event.str=my_id.id.task;
  
  l4events_send(1, &event, &eventnr, L4EVENTS_SEND_ACK);
  l4events_get_ack(eventnr, L4_IPC_NEVER);
*/  
  return 0;
}
