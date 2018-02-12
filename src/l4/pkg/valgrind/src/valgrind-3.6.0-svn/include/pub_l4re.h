/*--------------------------------------------------------------------*/
/*--- L4 stuff                                          pub_l4re.h ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2008 Julian Seward
      jseward@acm.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __PUB_L4RE_H
#define __PUB_L4RE_H

//#include "pub_core_ume.h"
#include <l4/re/env.h>
#undef offsetof
#include <stddef.h>
#include <unistd.h>
#include <l4/sys/kdebug.h>

#include "pub_tool_basics.h"
/*
 * Therefore coregrind knows which tool actually runs, 
 * every tool must export his name.
 */
extern HChar *VG_(toolname);

/*
 * Function-Wrapping stuff
 */
struct vg_l4re_capfunc_info {
	void (*vg_cap_alloc)(ThreadId tid, long idx);
	void (*vg_cap_free)(ThreadId tid, long idx);
};

void vg_cap_alloc(long, long);
void vg_cap_free(long, long);

/*--------------------------------------------------------------------*/
/*--- L4 VRM                                                       ---*/
/*--------------------------------------------------------------------*/

extern l4_cap_idx_t l4_vcap_thread;


struct vrm_area {
    l4_addr_t     start;
    l4_addr_t     end;
    unsigned char flags;
};

struct vrm_region {
    l4_addr_t     start;
    l4_addr_t     end;
    l4_addr_t     size;
    unsigned char flags;
    l4_addr_t     offset_in_ds;
    l4_cap_idx_t  ds;
};

struct vrm_region_lists {
    l4_addr_t  min_addr;
    l4_addr_t  max_addr;
    unsigned  region_count;
    unsigned  area_count;
    struct vrm_region *regions;
    struct vrm_area   *areas;
};


void vrm_get_lists(struct vrm_region_lists *rlist, unsigned num_regions, unsigned num_areas);

/*
 * Start the L4Re vcap thread
 */
void l4re_vcap_start_thread(void);
EXTERN_C void vcap_init(void);

struct ume_auxv;

/*
 * Instrument L4Re env with vcap modifications
 *
 * \param envp - old environment
 * \return pointer to new environment
 */
void *l4re_vcap_modify_env(struct ume_auxv *envp, Addr client_l4re_env_addr);

extern void *client_env;
extern unsigned int client_env_size;

#endif   // __PUB_L4RE_H

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
