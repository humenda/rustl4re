/*
 * \brief   DOpE Window layout module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module defines the layout of the window's
 * control elements.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopestd.h"
#include "button.h"
#include "winlayout.h"
#include "window.h"
#include "userstate.h"

#define WE_L      0x01
#define WE_O      0x02
#define WE_R      0x04
#define WE_U      0x08
#define WE_FULLER 0x10
#define WE_CLOSER 0x20
#define WE_TITLE  0x40

static struct button_services       *but;
static struct userstate_services    *userstate;

static s32 bsize = 5;     /* border size */
static s32 tsize = 17;    /* title size */

int init_winlayout(struct dope_services *d);

/************************
 *** HELPER FUNCTIONS ***
 ************************/

static WIDGET *new_button(WIDGET *next,long x,long y,long w,long h,char *txt,void *clic,long context) {
	BUTTON *nb = but->create();
	nb->gen->set_x((WIDGET *)nb, x);
	nb->gen->set_y((WIDGET *)nb, y);
	nb->gen->set_w((WIDGET *)nb, w);
	nb->gen->set_h((WIDGET *)nb, h);
	nb->gen->set_evforward((WIDGET *)nb, 0);
	nb->gen->set_context((WIDGET *)nb, (void *)context);
	nb->gen->set_next((WIDGET *)nb, next);
	nb->gen->set_selectable((WIDGET *)nb, 0);
	nb->but->set_click(nb, clic);
	nb->but->set_text(nb, txt);
	nb->but->set_font(nb, 2);
	nb->but->set_style(nb, 0);
	nb->but->set_free_w(nb, 1);
	nb->but->set_free_h(nb, 1);
	nb->but->set_pad_x(nb, 0);
	nb->but->set_pad_y(nb, 0);
	return (WIDGET *)nb;
}

static void resize_callback(BUTTON *b) {
	WINDOW *w;
	if (!b) return;
	w = (WINDOW *)b->gen->get_window((WIDGET *)b);
	if (!w) return;
	w->win->handle_resize(w,(WIDGET *)b);
}


static void move_callback(BUTTON *b) {
	WINDOW *w;
	if (!b) return;
	w = (WINDOW *)b->gen->get_window((WIDGET *)b);
	if (!w) return;
	w->win->handle_move(w,(WIDGET *)b);
}

/*************************
 *** SERVICE FUNCTIONS ***
 *************************/


static WIDGET *create_win_elements(s32 elements, int x, int y, int width, int height) {
	s32 dx,dy,dw,dh;
	WIDGET *first=NULL;

	dx = bsize;
	dy = bsize + tsize;
	dw = width  - 2*bsize;
	dh = height - 2*bsize - tsize;

	if ((elements & WIN_CLOSER) | (elements & WIN_FULLER) | (elements & WIN_TITLE)) {
		dy -= tsize; dh += tsize;
	}

	if (elements & WIN_BORDERS) {
		dy -= bsize; dx -= bsize; dw += 2*bsize; dh += 2*bsize;
	}

	dx += x;
	dy += y;

	if (elements & WIN_BORDERS) {
		first=new_button(first,dx,dy+bsize,bsize,dh-bsize-bsize,NULL,resize_callback,WE_L);
		first=new_button(first,dx,dy,bsize,bsize,NULL,resize_callback,WE_L+WE_O);
		first=new_button(first,dx+bsize,dy,dw-bsize-bsize,bsize,NULL,resize_callback,WE_O);
		first=new_button(first,dx+dw-bsize,dy,bsize,bsize,NULL,resize_callback,WE_O+WE_R);
		first=new_button(first,dx+dw-bsize,dy+bsize,bsize,dh-bsize-bsize,NULL,resize_callback,WE_R);
		first=new_button(first,dx+dw-bsize,dy+dh-bsize,bsize,bsize,NULL,resize_callback,WE_R+WE_U);
		first=new_button(first,dx+bsize,dy+dh-bsize,dw-2*bsize,bsize,NULL,resize_callback,WE_U);
		first=new_button(first,dx,dy+dh-bsize,bsize,bsize,NULL,resize_callback,WE_L+WE_U);
		dx+=bsize;dy+=bsize;dw-=bsize*2;dh-=bsize*2;
	}

	if (elements & WIN_CLOSER) {
		first=new_button(first,dx,dy,tsize,tsize,"",NULL,WE_CLOSER);
		dx+=tsize;dw-=tsize;
	}

	if (elements & WIN_FULLER) {
		first=new_button(first,dx+dw-tsize,dy,tsize,tsize,"",NULL,WE_FULLER);
		dw-=tsize;
	}

	if ((elements & WIN_TITLE) | (elements & WIN_CLOSER) | (elements & WIN_FULLER)) {
		first=new_button(first,dx,dy,dw,tsize,"DOpE WINDOW",move_callback,WE_TITLE);
		dy+=tsize;dh-=tsize;
	}

	return first;
}


static void resize_win_elements(WIDGET *elem,s32 elem_mask,
                                int x, int y, int width, int height) {
	s32 tw = width, b = 0;

	if (elem_mask & WIN_BORDERS) {
		b = bsize;
		tw -= 2*b;
	}

	if (elem_mask & WIN_CLOSER) tw -= tsize;
	if (elem_mask & WIN_FULLER) tw -= tsize;

	while (elem) {

		switch ((unsigned long)elem->gen->get_context(elem)) {

		case WE_L:
			elem->gen->set_h(elem, height - bsize - bsize);
			elem->gen->updatepos(elem);
			break;
		case WE_O:
			elem->gen->set_w(elem, width - 2*bsize);
			elem->gen->updatepos(elem);
			break;
		case WE_R+WE_O:
			elem->gen->set_x(elem, x + width - bsize);
			elem->gen->updatepos(elem);
			break;
		case WE_R:
			elem->gen->set_x(elem, x + width - bsize);
			elem->gen->set_h(elem, height - 2*bsize);
			elem->gen->updatepos(elem);
			break;
		case WE_R+WE_U:
			elem->gen->set_x(elem, x + width - bsize);
			elem->gen->set_y(elem, y + height - bsize);
			elem->gen->updatepos(elem);
			break;
		case WE_U:
			elem->gen->set_w(elem, width - 2*bsize);
			elem->gen->set_y(elem, y + height - bsize);
			elem->gen->updatepos(elem);
			break;
		case WE_L+WE_U:
			elem->gen->set_y(elem, y + height - bsize);
			elem->gen->updatepos(elem);
			break;
		case WE_FULLER:
			elem->gen->set_x(elem, x + width - b - tsize);
			elem->gen->updatepos(elem);
			break;
		case WE_TITLE:
			elem->gen->set_w(elem, tw);
			elem->gen->updatepos(elem);
			break;
		}
		elem = elem->gen->get_next(elem);
	}
}


static void set_win_state(WIDGET *elem,s32 state) {
	while (elem) {
		if (state) ((BUTTON *)elem)->but->set_style((BUTTON *)elem,2);
		else ((BUTTON *)elem)->but->set_style((BUTTON *)elem,0);
		elem->gen->update(elem);
		elem=elem->gen->get_next(elem);
	}
}


static void set_win_title(WIDGET *elem,char *new_title) {
	while (elem) {
		if ((unsigned long)elem->gen->get_context(elem) == WE_TITLE) {
			((BUTTON *)elem)->but->set_text((BUTTON *)elem,new_title);
			((BUTTON *)elem)->gen->update((WIDGET *)elem);
			return;
		}
		elem=elem->gen->get_next(elem);
	}
}


static char *get_win_title(WIDGET *elem) {

	while (elem) {
		if ((unsigned long)elem->gen->get_context(elem) == WE_TITLE) {
			return ((BUTTON *)elem)->but->get_text((BUTTON *)elem);
		}
		elem=elem->gen->get_next(elem);
	}
	return NULL;
}


static s32 get_left_border(s32 elem_mask) {
	if (elem_mask & WIN_BORDERS) return bsize;
	return 0;
}


static s32 get_right_border(s32 elem_mask) {
	if (elem_mask & WIN_BORDERS) return bsize;
	return 0;
}


static s32 get_top_border(s32 elem_mask) {
	s32 top = 0;
	if (elem_mask & WIN_BORDERS) top+=bsize;
	if (elem_mask & (WIN_CLOSER|WIN_FULLER|WIN_TITLE)) top+=tsize;
	return top;

}


static s32 get_bottom_border(s32 elem_mask) {
	if (elem_mask & WIN_BORDERS) return bsize;
	return 0;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct winlayout_services services = {
	create_win_elements,
	resize_win_elements,
	set_win_title,
	get_win_title,
	set_win_state,
	get_left_border,
	get_right_border,
	get_top_border,
	get_bottom_border,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

extern int config_winborder;

int init_winlayout(struct dope_services *d) {

	but       = d->get_module("Button 1.0");
	userstate = d->get_module("UserState 1.0");

	bsize = config_winborder;
	if (bsize < 4)  bsize = 4;
	if (bsize > 16) bsize = 16;

	d->register_module("WinLayout 1.0",&services);
	return 1;
}
