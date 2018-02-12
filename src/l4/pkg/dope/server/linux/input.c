/*
 * \brief   DOpE pseudo input driver module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * It uses SDL for requesting the mouse state under
 * Linux.  All other modules should use this one to
 * get information about the mouse state. The hand-
 * ling of mouse cursor and its appeariance is done
 * by the 'Screen' component.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/*** GENERAL INCLUDES ***/
#include <stdlib.h>

/*** SDL INCLUDES ***/
#include <SDL/SDL.h>

/*** LOCAL INCLUDES ***/
#include "dopestd.h"
#include "event.h"
#include "input.h"

/*** INCLUDES EXPORTED BY DOPE ***/
#include "keycodes.h"

int init_input(struct dope_services *d);

extern int config_exg_z_y;   /* from startup.c */


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** MAP SDL SCANCODES (aka KEYCODES) TO DOpE EVENT KEYCODES ***/
long scancodes[512] = {
	DOPE_KEY_RESERVED, DOPE_KEY_RESERVED, DOPE_KEY_RESERVED, DOPE_KEY_RESERVED,
	DOPE_KEY_RESERVED, DOPE_KEY_RESERVED, DOPE_KEY_RESERVED, DOPE_KEY_RESERVED,
	DOPE_KEY_RESERVED, DOPE_KEY_ESC, DOPE_KEY_1, DOPE_KEY_2, DOPE_KEY_3,
	DOPE_KEY_4, DOPE_KEY_5, DOPE_KEY_6, DOPE_KEY_7, DOPE_KEY_8, DOPE_KEY_9,
	DOPE_KEY_0, DOPE_KEY_MINUS, DOPE_KEY_EQUAL, DOPE_KEY_BACKSPACE,
	DOPE_KEY_TAB, DOPE_KEY_Q, DOPE_KEY_W, DOPE_KEY_E, DOPE_KEY_R, DOPE_KEY_T,
	DOPE_KEY_Y, DOPE_KEY_U, DOPE_KEY_I, DOPE_KEY_O, DOPE_KEY_P,
	DOPE_KEY_LEFTBRACE, DOPE_KEY_RIGHTBRACE, DOPE_KEY_ENTER, DOPE_KEY_LEFTCTRL,
	DOPE_KEY_A, DOPE_KEY_S, DOPE_KEY_D, DOPE_KEY_F, DOPE_KEY_G, DOPE_KEY_H,
	DOPE_KEY_J, DOPE_KEY_K, DOPE_KEY_L, DOPE_KEY_SEMICOLON,
	DOPE_KEY_APOSTROPHE, DOPE_KEY_GRAVE, DOPE_KEY_LEFTSHIFT,
	DOPE_KEY_BACKSLASH, DOPE_KEY_Z, DOPE_KEY_X, DOPE_KEY_C, DOPE_KEY_V,
	DOPE_KEY_B, DOPE_KEY_N, DOPE_KEY_M, DOPE_KEY_COMMA, DOPE_KEY_DOT,
	DOPE_KEY_SLASH, DOPE_KEY_RIGHTSHIFT, DOPE_KEY_KPASTERISK, DOPE_KEY_LEFTALT,
	DOPE_KEY_SPACE, DOPE_KEY_CAPSLOCK, DOPE_KEY_F1, DOPE_KEY_F2, DOPE_KEY_F3,
	DOPE_KEY_F4, DOPE_KEY_F5, DOPE_KEY_F6, DOPE_KEY_F7, DOPE_KEY_F8,
	DOPE_KEY_F9, DOPE_KEY_F10, DOPE_KEY_NUMLOCK, DOPE_KEY_SCROLLLOCK,
	DOPE_KEY_KP7, DOPE_KEY_KP8, DOPE_KEY_KP9, DOPE_KEY_KPMINUS, DOPE_KEY_KP4,
	DOPE_KEY_KP5, DOPE_KEY_KP6, DOPE_KEY_KPPLUS, DOPE_KEY_KP1, DOPE_KEY_KP2,
	DOPE_KEY_KP3, DOPE_KEY_KP0, DOPE_KEY_KPDOT, DOPE_KEY_RESERVED,
	DOPE_KEY_RESERVED, DOPE_KEY_102ND, DOPE_KEY_F11, DOPE_KEY_F12,
	DOPE_KEY_HOME, DOPE_KEY_UP, DOPE_KEY_PAGEUP, DOPE_KEY_LEFT,
	DOPE_KEY_RESERVED, DOPE_KEY_RIGHT, DOPE_KEY_END, DOPE_KEY_DOWN,
	DOPE_KEY_PAGEDOWN, DOPE_KEY_INSERT, DOPE_KEY_DELETE, DOPE_KEY_KPENTER,
	DOPE_KEY_RIGHTCTRL, DOPE_KEY_PAUSE, DOPE_KEY_SYSRQ, DOPE_KEY_KPSLASH,
	DOPE_KEY_RIGHTALT, DOPE_KEY_RESERVED, DOPE_KEY_LEFTMETA,
	DOPE_KEY_RIGHTMETA, DOPE_KEY_COMPOSE,
};

static long map_scancode(long sc) {
	if ((sc < 0) || (sc > DOPE_KEY_MAX))
		return DOPE_KEY_RESERVED;

	return scancodes[sc];
}

/*** OLD: MAP SDL KEYSYMS TO DOpE EVENT KEYCODES ***/
static long map_keycode(SDLKey sk) {

	/*
	 * libSDL delivers key codes that are already mapped language specific.
	 * DOpE implements a custom keyboard mapping in the generic code. Thus,
	 * the keyboard mapping is done twice, which yields to bad results for
	 * the y and z keys on german keyboards.  We simply exchange the keys
	 * here and wait for your complaints.
	 */
	if (config_exg_z_y) {
		if (sk == SDLK_z)
			sk = SDLK_y;
		else if (sk == SDLK_y)
			sk = SDLK_z;
	}

	switch (sk) {
	case SDLK_BACKSPACE:    return DOPE_KEY_BACKSPACE;
	case SDLK_TAB:          return DOPE_KEY_TAB;
//  case SDLK_CLEAR:        return DOPE_KEY_CLEAR;
	case SDLK_RETURN:       return DOPE_KEY_ENTER;
	case SDLK_PAUSE:        return DOPE_KEY_PAUSE;
	case SDLK_ESCAPE:       return DOPE_KEY_ESC;
	case SDLK_SPACE:        return DOPE_KEY_SPACE;
//  case SDLK_EXCLAIM:      return DOPE_KEY_EXCLAIM;
//  case SDLK_QUOTEDBL:     return DOPE_KEY_QUOTEDBL;
//  case SDLK_HASH:         return DOPE_KEY_HASH;
//  case SDLK_DOLLAR:       return DOPE_KEY_DOLLAR;
//  case SDLK_AMPERSAND:    return DOPE_KEY_AMPERSAND;
//  case SDLK_QUOTE:        return DOPE_KEY_QUOTE;
//  case SDLK_LEFTPAREN:    return DOPE_KEY_LEFTPAREN;
//  case SDLK_RIGHTPAREN:   return DOPE_KEY_RIGHTPAREN;
//  case SDLK_ASTERISK:     return DOPE_KEY_ASTERISK;
//  case SDLK_PLUS:         return DOPE_KEY_PLUS;
	case SDLK_COMMA:        return DOPE_KEY_COMMA;
	case SDLK_MINUS:        return DOPE_KEY_MINUS;
	case SDLK_PERIOD:       return DOPE_KEY_DOT;
	case SDLK_SLASH:        return DOPE_KEY_SLASH;
	case SDLK_0:            return DOPE_KEY_0;
	case SDLK_1:            return DOPE_KEY_1;
	case SDLK_2:            return DOPE_KEY_2;
	case SDLK_3:            return DOPE_KEY_3;
	case SDLK_4:            return DOPE_KEY_4;
	case SDLK_5:            return DOPE_KEY_5;
	case SDLK_6:            return DOPE_KEY_6;
	case SDLK_7:            return DOPE_KEY_7;
	case SDLK_8:            return DOPE_KEY_8;
	case SDLK_9:            return DOPE_KEY_9;
//  case SDLK_COLON:        return DOPE_KEY_COLON;
	case SDLK_SEMICOLON:    return DOPE_KEY_SEMICOLON;
//  case SDLK_LESS:         return DOPE_KEY_LESS;
//  case SDLK_EQUALS:       return DOPE_KEY_EQUALS;
//  case SDLK_GREATER:      return DOPE_KEY_GREATER;
	case SDLK_QUESTION:     return DOPE_KEY_QUESTION;
//  case SDLK_AT:           return DOPE_KEY_AT;
//  case SDLK_LEFTBRACKET:  return DOPE_KEY_LEFTBRACKET;
	case SDLK_BACKSLASH:    return DOPE_KEY_BACKSLASH;
//  case SDLK_RIGHTBRACKET: return DOPE_KEY_RIGHTBRACKET;
//  case SDLK_CARET:        return DOPE_KEY_CARET;
//  case SDLK_UNDERSCORE:   return DOPE_KEY_UNDERSCORE;
//  case SDLK_BACKQUOTE:    return DOPE_KEY_BACKQUOTE;
	case SDLK_a:            return DOPE_KEY_A;
	case SDLK_b:            return DOPE_KEY_B;
	case SDLK_c:            return DOPE_KEY_C;
	case SDLK_d:            return DOPE_KEY_D;
	case SDLK_e:            return DOPE_KEY_E;
	case SDLK_f:            return DOPE_KEY_F;
	case SDLK_g:            return DOPE_KEY_G;
	case SDLK_h:            return DOPE_KEY_H;
	case SDLK_i:            return DOPE_KEY_I;
	case SDLK_j:            return DOPE_KEY_J;
	case SDLK_k:            return DOPE_KEY_K;
	case SDLK_l:            return DOPE_KEY_L;
	case SDLK_m:            return DOPE_KEY_M;
	case SDLK_n:            return DOPE_KEY_N;
	case SDLK_o:            return DOPE_KEY_O;
	case SDLK_p:            return DOPE_KEY_P;
	case SDLK_q:            return DOPE_KEY_Q;
	case SDLK_r:            return DOPE_KEY_R;
	case SDLK_s:            return DOPE_KEY_S;
	case SDLK_t:            return DOPE_KEY_T;
	case SDLK_u:            return DOPE_KEY_U;
	case SDLK_v:            return DOPE_KEY_V;
	case SDLK_w:            return DOPE_KEY_W;
	case SDLK_x:            return DOPE_KEY_X;
	case SDLK_y:            return DOPE_KEY_Y;
	case SDLK_z:            return DOPE_KEY_Z;
	case SDLK_DELETE:       return DOPE_KEY_DELETE;

	/* numeric keypad */
//  case SDLK_KP0:          return DOPE_KEY_KP0;
//  case SDLK_KP1:          return DOPE_KEY_KP1;
//  case SDLK_KP2:          return DOPE_KEY_KP2;
//  case SDLK_KP3:          return DOPE_KEY_KP3;
//  case SDLK_KP4:          return DOPE_KEY_KP4;
//  case SDLK_KP5:          return DOPE_KEY_KP5;
//  case SDLK_KP6:          return DOPE_KEY_KP6;
//  case SDLK_KP7:          return DOPE_KEY_KP7;
//  case SDLK_KP8:          return DOPE_KEY_KP8;
//  case SDLK_KP9:          return DOPE_KEY_KP9;
//  case SDLK_KP_PERIOD:    return DOPE_KEY_KP_DOT;
//  case SDLK_KP_DIVIDE:    return DOPE_KEY_KP_DIVIDE;
//  case SDLK_KP_MULTIPLY:  return DOPE_KEY_KP_MULTIPLY;
//  case SDLK_KP_MINUS:     return DOPE_KEY_KP_MINUS;
//  case SDLK_KP_PLUS:      return DOPE_KEY_KP_PLUS;
//  case SDLK_KP_ENTER:     return DOPE_KEY_KP_ENTER;
//  case SDLK_KP_EQUALS:    return DOPE_KEY_KP_EQUALS;

	/* arrows + home/end pad */
	case SDLK_UP:           return DOPE_KEY_UP;
	case SDLK_DOWN:         return DOPE_KEY_DOWN;
	case SDLK_RIGHT:        return DOPE_KEY_RIGHT;
	case SDLK_LEFT:         return DOPE_KEY_LEFT;
	case SDLK_INSERT:       return DOPE_KEY_INSERT;
	case SDLK_HOME:         return DOPE_KEY_HOME;
	case SDLK_END:          return DOPE_KEY_END;
	case SDLK_PAGEUP:       return DOPE_KEY_PAGEUP;
	case SDLK_PAGEDOWN:     return DOPE_KEY_PAGEDOWN;

	/* function keys */
	case SDLK_F1:           return DOPE_KEY_F1;
	case SDLK_F2:           return DOPE_KEY_F2;
	case SDLK_F3:           return DOPE_KEY_F3;
	case SDLK_F4:           return DOPE_KEY_F4;
	case SDLK_F5:           return DOPE_KEY_F5;
	case SDLK_F6:           return DOPE_KEY_F6;
	case SDLK_F7:           return DOPE_KEY_F7;
	case SDLK_F8:           return DOPE_KEY_F8;
	case SDLK_F9:           return DOPE_KEY_F9;
	case SDLK_F10:          return DOPE_KEY_F10;
	case SDLK_F11:          return DOPE_KEY_F11;
	case SDLK_F12:          return DOPE_KEY_F12;
	case SDLK_F13:          return DOPE_KEY_F13;
	case SDLK_F14:          return DOPE_KEY_F14;
	case SDLK_F15:          return DOPE_KEY_F15;

	/* key state modifier keys */
	case SDLK_NUMLOCK:      return DOPE_KEY_NUMLOCK;
	case SDLK_CAPSLOCK:     return DOPE_KEY_CAPSLOCK;
//  case SDLK_SCROLLOCK:    return DOPE_KEY_SCROLLOCK;
	case SDLK_RSHIFT:       return DOPE_KEY_RIGHTSHIFT;
	case SDLK_LSHIFT:       return DOPE_KEY_LEFTSHIFT;
	case SDLK_RCTRL:        return DOPE_KEY_RIGHTCTRL;
	case SDLK_LCTRL:        return DOPE_KEY_LEFTCTRL;
	case SDLK_RALT:         return DOPE_KEY_RIGHTALT;
	case SDLK_LALT:         return DOPE_KEY_LEFTALT;
	case SDLK_RMETA:        return DOPE_KEY_RIGHTALT;
	case SDLK_LMETA:        return DOPE_KEY_LEFTALT;
//  case SDLK_LSUPER:       return DOPE_KEY_LSUPER;
//  case SDLK_RSUPER:       return DOPE_KEY_RSUPER;
//  case SDLK_MODE:         return DOPE_KEY_MODE;
//  case SDLK_COMPOSE:      return DOPE_KEY_COMPOSE;

	/* miscellaneous function keys */
//  case SDLK_HELP:         return DOPE_KEY_HELP;
//  case SDLK_PRINT:        return DOPE_KEY_PRINT;
//  case SDLK_SYSREQ:       return DOPE_KEY_SYSREQ;
//  case SDLK_BREAK:        return DOPE_KEY_BREAK;
//  case SDLK_MENU:         return DOPE_KEY_MENU;
//  case SDLK_POWER:        return DOPE_KEY_POWER;
//  case SDLK_EURO:         return DOPE_KEY_EURO;
//  case SDLK_UNDO:         return DOPE_KEY_UNDO;

	default: return 0;
	}
	return 0;
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** GET NEXT EVENT OF EVENT QUEUE ***
 *
 * \return  0 if there is no pending event
 *          1 if there an event was returned in out parameter e
 */
static int get_event(EVENT *e) {
	static SDL_Event sdl_event;
	static int new_mx, new_my;
	static int old_mx, old_my;

	SDL_GetMouseState(&new_mx, &new_my);

	if (!SDL_PollEvent(&sdl_event)) return 0;

	switch (sdl_event.type) {
		case SDL_KEYUP:
			e->type = EVENT_RELEASE;
			e->code = map_keycode(sdl_event.key.keysym.sym);
			break;

		case SDL_KEYDOWN:
			e->type = EVENT_PRESS;
//			e->code = map_keycode(sdl_event.key.keysym.sym);
			e->code = map_scancode(sdl_event.key.keysym.scancode);
			break;

		case SDL_QUIT:
			exit(0);

		case SDL_MOUSEBUTTONDOWN:
			switch (sdl_event.button.button) {
				case SDL_BUTTON_LEFT:
					e->type = EVENT_PRESS;
					e->code = DOPE_BTN_LEFT;
					break;
				case SDL_BUTTON_RIGHT:
					e->type = EVENT_PRESS;
					e->code = DOPE_BTN_RIGHT;
					break;
			}
			break;

		case SDL_MOUSEBUTTONUP:
			switch (sdl_event.button.button) {
				case SDL_BUTTON_LEFT:
					e->type = EVENT_RELEASE;
					e->code = DOPE_BTN_LEFT;
					break;
				case SDL_BUTTON_RIGHT:
					e->type = EVENT_RELEASE;
					e->code = DOPE_BTN_RIGHT;
					break;
			}
			break;

		case SDL_MOUSEMOTION:
			e->type = EVENT_ABSMOTION;
			e->abs_x = new_mx;
			e->abs_y = new_my;
			old_mx = new_mx;
			old_my = new_my;
			break;
	}
	return 1;
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct input_services input = {
	get_event,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_input(struct dope_services *d) {
	d->register_module("Input 1.0", &input);
	return 1;
}
