/**
 * \file	con/server/src/main.c
 * \brief	'main' of the con server
 *
 * \date	2001
 * \author	Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de> */
/*
 * (c) 2001-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* L4 includes */
#include <l4/l4con/l4con.h>
#include <l4/l4con/l4con_ev.h>
#include <l4/input/libinput.h>
#include <l4/sys/kdebug.h>
#include <l4/util/parse_cmd.h>
#include <l4/sys/err.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/namespace.h>
#include <l4/sys/thread.h>
#if defined(ARCH_x86) || defined(ARCH_amd64)
#include <l4/util/rdtsc.h>
#endif

/* LibC includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <pthread-l4.h>

/* local includes */
#include "config.h"
#include "ev.h"
#include "events.h"
#include "gmode.h"
#include "l4con.h"
#include "main.h"
#include "vc.h"
#include "srv.h"

int noaccel;				/* 1=disable pslim HW acceleration */
int nolog;				/* 1=disable logging to logserver */
int pan;				/* 1=pan display to 4MB boundary */
int use_fastmemcpy = 1;			/* 1=fast memcpy using SSE2 */
int cpu_load;
int cpu_load_history;
int vbemode;

struct l4con_vc *vc[MAX_NR_L4CONS];	/* virtual consoles */
int fg_vc = -1;				/* current foreground vc */
int want_vc = 0;			/* the vc we want to switch to */
					/* if we set want_vc = -1, con_loop
					 * must look for an open vc in list;
					 * don't switch to USED VCs, only to
					 * that in OUT/INOUT mode */
int update_id;				/* 1=redraw vc id */
pthread_mutex_t want_vc_lock = PTHREAD_MUTEX_INITIALIZER;/* mutex for want_vc */
l4_cap_idx_t ev_partner_l4id = L4_INVALID_CAP;/* current event handler */
l4_cap_idx_t vc_partner_l4id = L4_INVALID_CAP;/* current active client */
l4_uint8_t vc_mode = CON_CLOSED;		/* mode of current console */
l4re_util_video_goos_fb_t goosfb;


static void do_switch(int i_new, int i_old);

static void
fast_memcpy(l4_uint8_t *dst, l4_uint8_t *src, l4_size_t size)
{
#if defined(ARCH_x86) || defined(ARCH_amd64)
  l4_umword_t dummy;

  asm volatile ("cld ; rep movsl"
		:"=S"(dummy), "=D"(dummy), "=c"(dummy)
                :"S"(src), "D"(dst),"c"(size/sizeof(l4_umword_t)));
#else
  memcpy(dst, src, size);
#endif
}

/* fast memcpy using movntq (moving using non-temporal hint)
 * cache line size is 32 bytes */
static void
fast_memcpy_mmx2_32(l4_uint8_t *dst, l4_uint8_t *src, l4_size_t size)
{
#if defined(ARCH_x86) || defined(ARCH_amd64)
  l4_mword_t dummy;

  /* don't execute emms in side the timer loop because at this point the
   * fpu state is lazy allocated so we may have a kernel entry here */
  asm ("emms");

  asm volatile ("lea    (%%esi,%%edx,8),%%esi     \n\t"
		"lea    (%%edi,%%edx,8),%%edi     \n\t"
		"neg    %%edx                     \n\t"
		".align 8                         \n\t"
		"0:                               \n\t"
		"mov    $64,%%ecx                 \n\t"
		".align 16                        \n\t"
		"# block prefetch 4096 bytes      \n\t"
		"1:                               \n\t"
		"movl   (%%esi,%%edx,8),%%eax     \n\t"
		"movl   32(%%esi,%%edx,8),%%eax   \n\t"
		"add    $8,%%edx                  \n\t"
		"dec    %%ecx                     \n\t"
		"jnz    1b                        \n\t"
		"sub    $(32*16),%%edx            \n\t"
		"mov    $128,%%ecx                \n\t"
		".align 16                        \n\t"
		"2: # copy 4096 bytes             \n\t"
		"movq   (%%esi,%%edx,8),%%mm0     \n\t"
		"movq   8(%%esi,%%edx,8),%%mm1    \n\t"
		"movq   16(%%esi,%%edx,8),%%mm2   \n\t"
		"movq   24(%%esi,%%edx,8),%%mm3   \n\t"
		"movntq %%mm0,(%%edi,%%edx,8)     \n\t"
		"movntq %%mm1,8(%%edi,%%edx,8)    \n\t"
		"movntq %%mm2,16(%%edi,%%edx,8)   \n\t"
		"movntq %%mm3,24(%%edi,%%edx,8)   \n\t"
		"add    $4,%%edx                  \n\t"
		"dec    %%ecx                     \n\t"
		"jnz    2b                        \n\t"
		"or     %%edx,%%edx               \n\t"
		"jnz    0b                        \n\t"
		"sfence                           \n\t"
		"emms                             \n\t"
		: "=d" (dummy)
		: "S" (src), "D" (dst), "d" (size/8)
		: "eax", "ebx", "ecx", "memory");
#else
  memcpy(dst, src, size);
#endif
}

/** Find first available (non-opened) vc in list. */
static int
get_free_vc(void)
{
  int i;

  for (i=0; i < MAX_NR_L4CONS; i++)
    {
      if (vc[i]->mode == CON_CLOSED)
	return i;
    }

  printf("Ooops, all vcs occupied\n");
  return -CON_EFREE;
}

/** send con_event redraw.
 * @pre have want_vc_lock */
static void
redraw_vc(int vcnum)
{
   struct l4input ev_struct;

   if (vcnum >= MAX_NR_L4CONS)
     return;

   ev_struct.time = l4_kip_clock(l4re_kip());
   ev_struct.type = EV_CON;
   ev_struct.code = EV_CON_REDRAW;
   ev_struct.value = 0;

   send_event_client(vc[vcnum], &ev_struct);
}


/** send con_event background. */
static void
background_vc(int vcnum)
{
   struct l4input ev_struct;

   if (vcnum >= MAX_NR_L4CONS || vcnum < 0)
     return;

   ev_struct.time = l4_kip_clock(l4re_kip());
   ev_struct.type = EV_CON;
   ev_struct.code = EV_CON_BACKGROUND;
   ev_struct.value = 0;

   send_event_client(vc[vcnum], &ev_struct);

   vc[vcnum]->fb_mapped = 0;
}

/** Announce that we want to switch to another console.
 *  This function should not block for a longer time. */
void
request_vc(int nr)
{
  pthread_mutex_lock(&want_vc_lock);
  want_vc = nr;
  pthread_mutex_unlock(&want_vc_lock);
}

/** Announce that we want to switch to another console.
 *  This function should not block for a longer time. */
void
request_vc_delta(int delta)
{
  int new_vc;

  if (delta == 0)
    return;
  pthread_mutex_lock(&want_vc_lock);
  new_vc = fg_vc + delta;
  for (;;)
    {
      if (new_vc <= 0)
	new_vc = MAX_NR_L4CONS-1;
      else if (new_vc >= MAX_NR_L4CONS)
	new_vc = 1;
      if (new_vc == fg_vc || (vc[new_vc]->mode & CON_OUT))
	break;
      new_vc += delta < 0 ? -1 : +1;
    }
  want_vc = new_vc;
  pthread_mutex_unlock(&want_vc_lock);
}

/** Switch to other console (want_vc != fg_vc).
 * @pre have want_vc_lock */
static void
switch_vc(void)
{
  int i;

  if ((want_vc > 0) && (want_vc & 0x1000))
    {
      /* special case: close current console */
      printf("Still missing: %s %d\n", __func__, __LINE__);
#ifdef FIXME
      CORBA_Environment env = dice_default_environment;
      pthread_mutex_unlock(&want_vc_lock);
      con_vc_close_call(&(vc[want_vc & ~0x1000]->vc_l4id), &env);
      pthread_mutex_lock(&want_vc_lock);
#endif
      want_vc = -1;
    }

  if (want_vc < 0)
    {
      struct l4con_vc *new;

      if ((new = vc[fg_vc]))
	{
	  for (; (new = new->prev); )
	    {
	      if ((new->mode & CON_OUT) && !(new->mode & CON_MASTER))
		{
		  i = new->vc_number;
		  goto found;
		}
	    }
	}

      for (i=0; i<MAX_NR_L4CONS; i++)
	{
	  if ((vc[i]->mode & CON_OUT) && !(vc[i]->mode & CON_MASTER))
	    {
found:
	      printf("switch to vc %02d\n", i);

	      /* need fb_lock for that */
	      do_switch(i, 0);
	      redraw_vc(i);
	      want_vc = fg_vc = i;
	      break;
	    }
	}
      if (i == MAX_NR_L4CONS)
	{
	  printf("All vc's closed\n");
	  /* switch to vc 0 (master) */
	  do_switch(0, -1);
	  want_vc = fg_vc = 0;
	}
    }
  else
    {
      /* request for explicit switch (event or open) */
      if (want_vc >= MAX_NR_L4CONS)
	{
	  /* return to sane state */
	  want_vc         = fg_vc;
	  ev_partner_l4id = vc[fg_vc]->ev_partner_l4id;
          vc_partner_l4id = vc[fg_vc]->vc_partner_l4id;
	  vc_mode         = vc[fg_vc]->mode;
	  return;
	}

      /* check if want_vc is open */
      if (vc[want_vc]->mode & CON_OUT)
	{
	  do_switch(want_vc, fg_vc);
	  redraw_vc(want_vc);

	  /* set a sane state */
	  fg_vc = want_vc;
	}
      else
	{
	  /* set a sane state */
	  want_vc = fg_vc;
	}
    }
}

static void
do_switch(int i_new, int i_old)
{
  struct l4con_vc *old = (i_old >= 0) ? vc[i_old] : 0;
  struct l4con_vc *new = (i_new >= 0) ? vc[i_new] : 0;

  /* save old vc */
  if (old != 0)
    {
      pthread_mutex_lock(&old->fb_lock);
      if (old->save_restore && old->vfb)
	{
	  /* save screen */
	  if (use_fastmemcpy && (new->vfb_size % 4096 == 0))
	    fast_memcpy_mmx2_32(old->vfb, vis_vmem, old->vfb_size);
	  else
	    fast_memcpy        (old->vfb, vis_vmem, old->vfb_size);
	}
      old->fb       = old->vfb_in_server ? old->vfb : 0;
      old->pan_xofs = old->vfb_in_server ? 0        : pan_offs_x;
      old->pan_yofs = old->vfb_in_server ? 0        : pan_offs_y;
      old->do_copy  = bg_do_copy;
      old->do_fill  = bg_do_fill;
      old->do_sync  = bg_do_sync;
      old->do_drty  = 0;
      old->next     = new;
      pthread_mutex_unlock(&old->fb_lock);
    }

  /* setup new vc */
  if (new != 0)
    {
      pthread_mutex_lock(&new->fb_lock);
      new->fb       = gr_vmem;
      new->pan_xofs = pan_offs_x;
      new->pan_yofs = pan_offs_y;
      new->do_copy  = fg_do_copy;
      new->do_fill  = fg_do_fill;
      new->do_sync  = fg_do_sync;
      new->do_drty  = fg_do_drty;
      new->prev     = old;

      if (i_new == 0 || !new->save_restore || !new->vfb)
        {
          /* We clear the screen here for security reasons: If save_restore
           * is false the client is responsible for updating the screen when
           * it receives the EV_CON_REDRAW event. A malicous client could
           * "forget" to respond but now has the input focus. */
          vc_clear(new);
        }

      if (new->save_restore && new->vfb)
	{
	  /* restore screen */
	  if (use_fastmemcpy && (new->vfb_size % 4096 == 0))
	    fast_memcpy_mmx2_32(vis_vmem, new->vfb, new->vfb_size);
	  else
	    fast_memcpy        (vis_vmem, new->vfb, new->vfb_size);
	}

      /* force redraw of changed screen content (needed by VMware) */
      if (new->do_drty)
	new->do_drty(0, 0, fb_info.width, fb_info.height);

      pthread_mutex_unlock(&new->fb_lock);

      update_id = 1;
    }

  /* tell old console that we flash its video memory */
  if (i_old)
    background_vc(i_old);

  if (old && old->fb_mapped)
  {
    /* Flush video memory of old console.
     * XXX This does not work on current Fiasco implementation because the
     * mapping database does not record mappings of pages beyond end of
     * physical RAM. */
    l4_addr_t map_addr;

    for (map_addr=(l4_addr_t)gr_vmem;
         map_addr+L4_SUPERPAGESIZE < (l4_addr_t)gr_vmem_maxmap;
         map_addr+=L4_SUPERPAGESIZE)
      {
#ifdef FIXME
        l4_fpage_unmap(l4_fpage(map_addr, L4_LOG2_SUPERPAGESIZE, 0, 0),
                       L4_FP_FLUSH_PAGE | L4_FP_OTHER_SPACES);
#endif
      }
  }
  ev_partner_l4id = vc[i_new]->ev_partner_l4id;
  vc_partner_l4id = vc[i_new]->vc_partner_l4id;
  vc_mode         = vc[i_new]->mode;
}

/******************************************************************************
 * con_if - IDL server functions                                              *
 ******************************************************************************/

/** \brief Query for new (not opened) virtual console
 *
 * in:  request	      ... Flick request structure
 *      sbuf_size     ... max IPC string buffer
 * out: vcid          ... threadid of vc_thread
 *      _ev           ... Flick exception (unused)
 * ret: 0             ... success
 *      -???          ... if no VC unused */
#ifdef FIXME
long
con_if_openqry_component (CORBA_Object _dice_corba_obj,
                          unsigned long sbuf1_size,
                          unsigned long sbuf2_size,
                          unsigned long sbuf3_size,
                          unsigned char priority,
                          l4_threadid_t *vcid,
                          short vfbmode,
                          CORBA_Server_Environment *_dice_corba_env)
{
  l4thread_t vc_tid;
  l4_threadid_t vc_l4id, dummy;
  l4dm_dataspace_t ds;
  l4_offs_t ds_offs;
  l4_addr_t ds_map_addr;
  l4_size_t ds_map_size;
  int vc_num;
  char name[32];

  /* check sbuf_size */
  if (!sbuf1_size
      || (sbuf2_size != 0 && sbuf3_size == 0)
      || (sbuf2_size == 0 && sbuf3_size != 0)
      || (sbuf1_size + sbuf2_size + sbuf3_size) > CONFIG_MAX_SBUF_SIZE)
    {
      printf("Wrong string buffer size\n");
      return -CON_EXPARAM;
    }

  /* find first available (non-opened) vc in list */
  if ((vc_num = get_free_vc()) == -CON_EFREE)
    return vc_num;

  /* allocate memory for sbuf */
  sprintf(name, "ipcbuf1 for "l4util_idfmt"", l4util_idstr(*_dice_corba_obj));
  if (!(vc[vc_num]->sbuf1 =
	l4dm_mem_allocate_named(sbuf1_size, L4RM_MAP, name)))
    {
      printf("Ooops, not enough memory for 1st string buffer\n");
      return -CON_ENOMEM;
    }

  vc[vc_num]->sbuf2 = 0;
  if (sbuf2_size > 0)
    {
      sprintf(name, "ipcbuf2 for "l4util_idfmt"",
	  l4util_idstr(*_dice_corba_obj));
      if (sbuf2_size)
	{
	  if (!(vc[vc_num]->sbuf2 =
		l4dm_mem_allocate_named(sbuf2_size, L4RM_MAP, name)))
	    {
	      printf("Ooops, not enough memory for 2nd string buffer\n");
	      l4dm_mem_release(vc[vc_num]->sbuf1);
	      return -CON_ENOMEM;
	    }
	}
    }

  vc[vc_num]->sbuf3 = 0;
  if (sbuf3_size > 0)
    {
      sprintf(name, "ipcbuf3 for "l4util_idfmt"",
	      l4util_idstr(*_dice_corba_obj));
      if (sbuf3_size)
	{
	  if (!(vc[vc_num]->sbuf3 =
		l4dm_mem_allocate_named(sbuf3_size, L4RM_MAP, name)))
	    {
	      printf("Ooops, not enough memory for 3rd string buffer\n");
	      l4dm_mem_release(vc[vc_num]->sbuf1);
	      l4dm_mem_release(vc[vc_num]->sbuf2);
	      return -CON_ENOMEM;
	    }
	}
    }

  vc[vc_num]->fb              = 0;
  vc[vc_num]->vfb             = 0;
  vc[vc_num]->vfb_in_server   = vfbmode;
  vc[vc_num]->mode            = CON_OPENING;
  vc[vc_num]->vc_partner_l4id = *_dice_corba_obj;
  vc[vc_num]->sbuf1_size      = sbuf1_size;
  vc[vc_num]->sbuf2_size      = sbuf2_size;
  vc[vc_num]->sbuf3_size      = sbuf3_size;
  vc[vc_num]->fb_mapped       = 0;

  if (vfbmode)
    vc[vc_num]->save_restore  = 1;

  sprintf(name, ".vc-%.2d", vc_num);
  vc_tid = l4thread_create_long(L4THREAD_INVALID_ID,
				(l4thread_fn_t) vc_loop, name,
				L4THREAD_INVALID_SP, L4THREAD_DEFAULT_SIZE,
				priority, (void *) vc[vc_num],
				L4THREAD_CREATE_SYNC);

  vc_l4id = l4thread_l4_id(vc_tid);

  vc[vc_num]->vc_l4id         = vc_l4id;
  vc[vc_num]->ev_partner_l4id = L4_INVALID_CAP;

  // transfer ownership of ipc buffers to client thread
  if (L4RM_REGION_DATASPACE == l4rm_lookup(vc[vc_num]->sbuf1, &ds_map_addr,
                                           &ds_map_size, &ds, &ds_offs, &dummy))
    l4dm_transfer(&ds, vc_l4id);
  if (L4RM_REGION_DATASPACE == l4rm_lookup(vc[vc_num]->sbuf2, &ds_map_addr,
                                           &ds_map_size, &ds, &ds_offs, &dummy))
    l4dm_transfer(&ds, vc_l4id);
  if (L4RM_REGION_DATASPACE == l4rm_lookup(vc[vc_num]->sbuf3, &ds_map_addr,
                                           &ds_map_size, &ds, &ds_offs, &dummy))
    l4dm_transfer(&ds, vc_l4id);

  /* reply vc_thread id */
  *vcid  = vc_l4id;

  return 0;
}
#endif

/*static*/ int
vc_open(struct l4con_vc *vc, l4_uint8_t mode, l4_cap_idx_t ev_handler);

long
con_if_open_component(short vfbmode)
{
  //l4thread_t vc_tid;
  //l4_threadid_t vc_l4id, dummy;
  //l4dm_dataspace_t ds;
  //l4_addr_t ds_offs;
  //l4_addr_t ds_map_addr;
  //l4_size_t ds_map_size;
  long r;
  int vc_num;
  //char name[32];

  /* find first available (non-opened) vc in list */
  if ((vc_num = get_free_vc()) == -CON_EFREE)
    return vc_num;

  vc[vc_num]->fb              = 0;
  vc[vc_num]->vfb             = 0;
  vc[vc_num]->vfb_in_server   = vfbmode;
  vc[vc_num]->mode            = CON_OPENING;
  //vc[vc_num]->vc_partner_l4id = *_dice_corba_obj;
  //vc[vc_num]->sbuf1_size      = sbuf1_size;
  //vc[vc_num]->sbuf2_size      = sbuf2_size;
  //vc[vc_num]->sbuf3_size      = sbuf3_size;
  vc[vc_num]->fb_mapped       = 0;

  if (vfbmode)
    vc[vc_num]->save_restore  = 1;

#if 0
  sprintf(name, ".vc-%.2d", vc_num);
  vc_tid = l4thread_create_long(L4THREAD_INVALID_ID,
				(l4thread_fn_t) vc_loop, name,
				L4THREAD_INVALID_SP, L4THREAD_DEFAULT_SIZE,
				priority, (void *) vc[vc_num],
				L4THREAD_CREATE_SYNC);

  vc_l4id = l4thread_l4_id(vc_tid);

  vc[vc_num]->vc_l4id         = vc_l4id;
#endif
//  vc[vc_num]->ev_partner_l4id = L4_INVALID_CAP;

  //return vc_open(vc, mode & CON_INOUT, *ev_handler);

  if ((r = vc_open(vc[vc_num], CON_INOUT, L4_INVALID_CAP)))
    return r;

  create_event(vc[vc_num]);

  fill_out_info(vc[vc_num]);

  return vc_num;
}

#ifdef FIXME
long
con_if_screenshot_component (CORBA_Object _dice_corba_obj,
                             short vc_nr,
                             l4dm_dataspace_t *ds,
                             l4_uint32_t *xres,
                             l4_uint32_t *yres,
                             l4_uint32_t *bpp,
                             CORBA_Server_Environment *_dice_corba_env)
{
  void *addr;
  struct l4con_vc *vc_shoot;

  if (vc_nr >= MAX_NR_L4CONS)
    return -L4_EINVAL;

  if (vc_nr == 0)
    vc_nr = fg_vc;

  vc_shoot = vc[vc_nr];

  if (!(vc_shoot->mode & CON_INOUT))
    return -L4_EINVAL;

  if (!vc_shoot->vfb_in_server && vc_nr != fg_vc)
    return -L4_EINVAL;

  pthread_mutex_lock(&vc_shoot->fb_lock);

  if (!(addr = l4dm_mem_ds_allocate_named(vc_shoot->vfb_size, 0, "screenshot",
					  (l4dm_dataspace_t*)ds)))
    {
      printf("Allocating dataspace failed\n");
      return -CON_ENOMEM;
    }

  /* XXX consider situations where
   * bytes_per_line != bytes_per_pixel * xres !!! */
  memcpy(addr, vc_shoot->vfb_in_server ? vc_shoot->vfb : vc_shoot->fb,
         vc_shoot->vfb_size);

  pthread_mutex_unlock(&vc_shoot->fb_lock);

  *xres = fb_info.width;
  *yres = fb_info.height;
  *bpp  = fb_info.bits_per_pixel;

  l4rm_detach(addr);

  /* transfer ds to caller so that it can be freed again */
  l4dm_transfer((l4dm_dataspace_t*)ds, *_dice_corba_obj);

  return 0;
}
#endif

/** \brief Close all virtual consoles of a client
 *
 * in:  request	      ... Flick request structure
 *      client        ... id of client to close all vcs off
 * out: _ev           ... Flick exception (unused)
 * ret: 0             ... success
 *      -???          ... if some error occured on closing a vc */
#ifdef FIXME
long
con_if_close_all_component (CORBA_Object _dice_corba_obj,
                            const l4_threadid_t *client,
                            CORBA_Server_Environment *_dice_corba_env)
{
  int i;

  for (i=0; i<MAX_NR_L4CONS; i++)
    {
      if (vc[i]->mode != CON_CLOSED && vc[i]->mode != CON_CLOSING)
	{
	  /* don't use l4_task_equal here since we only know the task number */
	  if (vc[i]->vc_partner_l4id.id.task == client->id.task)
	    {
	      /* found console bound to client -- close it */
	      CORBA_Environment env = dice_default_environment;
	      int ret = con_vc_close_call(&(vc[i]->vc_l4id), &env);
	      if (ret || DICE_HAS_EXCEPTION(&env))
		printf("Error %d (env=%02x) closing app "l4util_idfmt
		       " service thread "l4util_idfmt,
		    ret, DICE_IPC_ERROR(&env), l4util_idstr(*client),
		    l4util_idstr(vc[i]->vc_l4id));
	    }
	}
    }

  return 0;
}
#endif

void
periodic_work(void)
{
  static l4_kernel_clock_t last_active_fast;
  static l4_kernel_clock_t last_active_slow;
  l4_kernel_clock_t clock = l4_kip_clock(l4re_kip());

  if (clock - last_active_fast >= 25000)
    {
      // switch to another console?
      pthread_mutex_lock(&want_vc_lock);
      if (want_vc != fg_vc)
	switch_vc();
      pthread_mutex_unlock(&want_vc_lock);

      // update status bar
      if (update_id && fg_vc >= 0)
	{
	  pthread_mutex_lock(&vc[fg_vc]->fb_lock);
	  vc_show_id(vc[fg_vc]);
	  pthread_mutex_unlock(&vc[fg_vc]->fb_lock);
	  update_id = 0;
	  // force updating load indicator
	  last_active_slow = clock - 1000000;
	}

      // update DROPS logo
      if (vc[fg_vc]->logo_x != 100000)
	{
	  pthread_mutex_lock(&vc[fg_vc]->fb_lock);
	  vc_show_drops_cscs_logo();
	  pthread_mutex_unlock(&vc[fg_vc]->fb_lock);
	}

      last_active_fast = clock;
    }

  if (clock - last_active_slow >= 1000000)
    {
      pthread_mutex_lock(&vc[fg_vc]->fb_lock);

      // update memory information
      vc_show_dmphys_poolsize(vc[fg_vc]);

      if (cpu_load)
	// update load indicator
	vc_show_cpu_load(vc[fg_vc]);

      pthread_mutex_unlock(&vc[fg_vc]->fb_lock);
      last_active_slow = clock;
    }
}


#ifdef ARCH_x86
#if 0
static int sse_faulted;

static int
rm_fault_sse(l4_msgtag_t tag, l4_utcb_t *utcb, l4_cap_idx_t src)
{
  extern char after_sse_insn[];
  sse_faulted = 1;
  utcb->exc.eip = (l4_umword_t)after_sse_insn;
  return L4RM_REPLY_EMPTY;
}
#endif
#endif // ARCH_x86

static void
check_fast_memcpy(void)
{
#if defined(ARCH_x86) || defined(ARCH_amd64)
  if (use_fastmemcpy)
    {
      /* assume AMD64 has SSE */
# ifdef ARCH_x86
      l4_uint64_t src, dst;
#warning Restore exception handler hook!!!
      //l4rm_set_unkown_fault_callback(rm_fault_sse);
      asm volatile("emms; movq (%0),%%mm0; movntq %%mm0,(%1); sfence; emms"
                   : : "r"(&src), "r"(&dst) , "m"(src) : "memory");
      asm volatile(".global after_sse_insn; after_sse_insn:" ::: "memory");
      //l4rm_set_unkown_fault_callback(NULL);
      if (1) //!sse_faulted)
        {
# endif
	  printf("Using fast memcpy.\n");
# ifdef ARCH_x86
	}
      else
	{
	  printf("Fast memcpy not supported by this CPU.\n");
	  use_fastmemcpy = 0;
	}
# endif
      return;
    }
#else
  use_fastmemcpy = 0;
#endif
  printf("Not using fast memcpy\n");
}

#if defined(ARCH_x86) || defined(ARCH_amd64)
#if 0
static int rdpmc_faulted;

static int
rm_fault_rdpmc(l4_msgtag_t tag, l4_utcb_t *utcb, l4_cap_idx_t src)
{
  extern char after_rdpmc_insn[];
  if (*(l4_uint16_t *)utcb->exc.eip != 0x330f) // sanity check
    {
      printf("Not faulted at rdpmc function (pc = %lx)", utcb->exc.eip);
      return L4RM_REPLY_NO_REPLY;
    }
  utcb->exc.eip = (l4_umword_t)after_rdpmc_insn;
  rdpmc_faulted = 1;
  return L4RM_REPLY_EMPTY;
}
#endif

static void
check_cpuload(void)
{
  if (cpu_load)
    {
#warning restore exception handler hook (PMC)!!!
     // l4rm_set_unkown_fault_callback(rm_fault_rdpmc);
      asm volatile("" ::: "memory");
      l4_rdpmc_32(0);
      asm volatile(".global after_rdpmc_insn; after_rdpmc_insn:" ::: "memory");
      //l4rm_set_unkown_fault_callback(NULL);
      if (1) //!rdpmc_faulted)
        {
	  printf("Enabling CPU load indicator\n"
	         "\033[32mALTGR+PAUSE switches CPU load indicator!\033[m\n");
	}
      else
	{
	  printf("Disabling CPU load indicator since rdpmc not available\n");
	  cpu_load = 0;
	}
    }
}
#else
static void
check_cpuload(void)
{
  cpu_load = 0;
}
#endif

#if 0
extern int console_puts(const char *s);
static void
my_LOG_outstring(const char *s)
{
  console_puts(s);
}
#endif

/* Make sure that the jiffies symbol is taken from libio.a not libinput.a. */
asm (".globl jiffies");

/** \brief Main function
 */
int
main(int argc, const char *argv[])
{
  int error;
  int use_events = 0;

#ifdef ARCH_arm
  noaccel = 1;
#endif

  if ((error = parse_cmdline(&argc, &argv,
		    'a', "noaccel", "disable hardware acceleration",
		    PARSE_CMD_SWITCH, 1, &noaccel,
#ifndef ARCH_arm
		    'c', "cpuload", "show CPU load using rdtsc and rdpmc(0)",
		    PARSE_CMD_SWITCH, 1, &cpu_load,
#endif
		    'e', "events", "use event server to free resources",
		    PARSE_CMD_SWITCH, 1, &use_events,
		    'l', "nolog", "don't connect to logserver",
		    PARSE_CMD_SWITCH, 1, &nolog,
		    'm', "nomouse", "don't transmit mouse events to clients",
		    PARSE_CMD_SWITCH, 1, &nomouse,
		    'b', "genabsevents", "generate absolute mouse events out of relatives",
		    PARSE_CMD_SWITCH, 1, &gen_abs_events,
		    'n', "nofastmemcpy", "force to not use fast memcpy",
		    PARSE_CMD_SWITCH, 0, &use_fastmemcpy,
		    'p', "pan", "use panning to restrict client window",
		    PARSE_CMD_SWITCH, 1, &pan,
		    ' ', "noshift", "no shift key for console switching",
		    PARSE_CMD_SWITCH, 1, &noshift,
		    'v', "vbemode", "set VESA mode",
		    PARSE_CMD_INT, 0, &vbemode,
		    0)))
    {
      switch (error)
	{
	case -1: printf("Bad parameter for parse_cmdline()\n"); break;
	case -2: printf("Out of memory in parse_cmdline()\n"); break;
	case -4: return 1;
	default: printf("Error %d in parse_cmdline()\n", error); break;
	}
    }

  cpu_load_history = 1;

  /* do not use logserver (in case the log goes to a console) */
#ifdef FIXME
  if (nolog)
    LOG_outstring = my_LOG_outstring;
#endif

  /* check if CPU supports fast memcpy */
  check_fast_memcpy();
  check_cpuload();

  vc_init();

  /* switch to master console: initial screen output (DROPS logo) */
  do_switch(0, -1);
  vc_show_id(vc[0]);
  fg_vc = want_vc = 0;
  update_id = 0;

  ev_init();

  /* start thread listening for exit events */
  //if (use_events)
  //  init_events();

  printf("Running. Video mode is %ldx%ld@%d.\n",
	 fb_info.width, fb_info.height,
	 l4re_video_bits_per_pixel(&fb_info.pixel_info));

  /* idl service loop */
  return server_loop();
}
