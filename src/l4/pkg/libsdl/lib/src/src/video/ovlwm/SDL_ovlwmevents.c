#include "SDL.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_ovlwmvideo.h"
#include "SDL_ovlwmevents.h"
#include <string.h>
#include <l4/thread/thread.h>
#include <l4/dope/dopelib.h>
#include <l4/overlay_wm/ovl_input.h>
#include <l4/log/l4log.h>

static SDLKey keymap[KEY_MAX];


/*** QUITE DUMMY ***/
void OVLWM_PumpEvents(_THIS) {
}


void OVLWM_InitOSKeymap(_THIS) {

	memset(keymap, 0, sizeof(keymap));

	keymap[KEY_ESC]        = SDLK_ESCAPE;
	keymap[KEY_ENTER]      = SDLK_RETURN;
	keymap[KEY_LEFTCTRL]   = SDLK_LCTRL;
	keymap[KEY_LEFTSHIFT]  = SDLK_LSHIFT;
	keymap[KEY_RIGHTSHIFT] = SDLK_RSHIFT;
	keymap[KEY_LEFTALT]    = SDLK_RALT;
	keymap[KEY_CAPSLOCK]   = SDLK_CAPSLOCK;
	keymap[KEY_F1]         = SDLK_F1;
	keymap[KEY_F2]         = SDLK_F2;
	keymap[KEY_F3]         = SDLK_F3;
	keymap[KEY_F4]         = SDLK_F4;
	keymap[KEY_F5]         = SDLK_F5;
	keymap[KEY_F6]         = SDLK_F6;
	keymap[KEY_F7]         = SDLK_F7;
	keymap[KEY_F8]         = SDLK_F8;
	keymap[KEY_F9]         = SDLK_F9;
	keymap[KEY_F10]        = SDLK_F10;
	keymap[KEY_F11]        = SDLK_F11;
	keymap[KEY_F12]        = SDLK_F12;
	keymap[KEY_NUMLOCK]    = SDLK_NUMLOCK;
	keymap[KEY_SCROLLLOCK] = SDLK_SCROLLOCK;
	keymap[KEY_KP0]        = SDLK_KP0;
	keymap[KEY_KP1]        = SDLK_KP1;
	keymap[KEY_KP2]        = SDLK_KP2;
	keymap[KEY_KP3]        = SDLK_KP3;
	keymap[KEY_KP4]        = SDLK_KP4;
	keymap[KEY_KP5]        = SDLK_KP5;
	keymap[KEY_KP6]        = SDLK_KP6;
	keymap[KEY_KP7]        = SDLK_KP7;
	keymap[KEY_KP8]        = SDLK_KP8;
	keymap[KEY_KP9]        = SDLK_KP9;
	keymap[KEY_KPMINUS]    = SDLK_KP_MINUS;
	keymap[KEY_KPPLUS]     = SDLK_KP_PLUS;
	keymap[KEY_KPDOT]      = SDLK_KP_PERIOD;
	keymap[KEY_KPENTER]    = SDLK_KP_ENTER;
	keymap[KEY_RIGHTCTRL]  = SDLK_RCTRL;
	keymap[KEY_KPSLASH]    = SDLK_KP_DIVIDE;
	keymap[KEY_RIGHTALT]   = SDLK_RALT;
	keymap[KEY_HOME]       = SDLK_HOME;
	keymap[KEY_UP]         = SDLK_UP;
	keymap[KEY_PAGEUP]     = SDLK_PAGEDOWN;
	keymap[KEY_LEFT]       = SDLK_LEFT;
	keymap[KEY_RIGHT]      = SDLK_RIGHT;
	keymap[KEY_END]        = SDLK_END;
	keymap[KEY_DOWN]       = SDLK_DOWN;
	keymap[KEY_PAGEDOWN]   = SDLK_PAGEUP;
	keymap[KEY_INSERT]     = SDLK_INSERT;
	keymap[KEY_DELETE]     = SDLK_DELETE;
	keymap[KEY_PAUSE]      = SDLK_PAUSE;
	keymap[KEY_LEFTMETA]   = SDLK_LSUPER;
	keymap[KEY_RIGHTMETA]  = SDLK_RSUPER;
	keymap[KEY_RIGHTALT]   = SDLK_MODE;
}


/*** CALLBACK THAT IS EXECUTED WHEN MOUSE MOVES ***/
static void motion_callback(int mx, int my) {
	SDL_PrivateMouseMotion(0, 0, mx, my);
}


/*** CALLBACK THAT IS EXECUTED WHEN BUTTONS ARE PRESSED OR RELEASED ***/
static void button_callback(int type, int code) {
	Uint8 state;
	SDL_keysym key;

	memset(&key, 0, sizeof(key));

	if (type == 1)
		state = SDL_PRESSED;
	else
		state = SDL_RELEASED;

	key.scancode = code;
	
	/* look into keymap */
	if (code>0 && code < KEY_MAX)
		key.sym = keymap[code];
	
//	/* or try to decode ascii */
//	if (!key.sym) {
//		if (this->hidden->have_app) {
//			key.sym = dope_get_ascii(this->hidden->app_id, code);
//			key.unicode = key.sym;
//		}
//	}

	/* catch some cases by hand */
	if (!key.sym) {
		switch (code) {
			case BTN_LEFT:
			case BTN_RIGHT:
			case BTN_MIDDLE:
			case BTN_SIDE:
			case BTN_EXTRA:
			case BTN_FORWARD:
			case BTN_BACK:
				SDL_PrivateMouseButton(state, code-BTN_MOUSE+1, 0, 0);
				break;
			default:
		}
	}
	if (key.sym) SDL_PrivateKeyboard(state, &key);
}


/*** INIT OVERLAY INPUT LIB AND ASSIGN CALLBACK FUNCTIONS ***/
void OVLWM_InstallEventHandler(_THIS) {
	if (this->hidden->have_app) {
		ovl_input_init(NULL);
		ovl_input_button(&button_callback);
		ovl_input_motion(&motion_callback);
	}
}


/*** REMOVE? - WE ARE HAPPY THAT IT IS UP AND RUNNING! ***/
void OVLWM_RemoveEventHandler(_THIS) {
}

