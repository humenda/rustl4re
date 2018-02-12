#include "SDL.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_dopevideo.h"
#include "SDL_dopeevents.h"
#include <string.h>
#include <l4/thread/thread.h>
#include <l4/dope/dopelib.h>
#include <l4/log/l4log.h>

static SDLKey keymap[DOPE_KEY_MAX];

typedef struct {
	char *obj;
	_THIS;
} DOPE_CallbackData;

static DOPE_CallbackData CD_vscr = {"vscr", NULL};
static DOPE_CallbackData CD_win  = {"win", NULL};

void DOPE_PumpEvents(_THIS) {
	INFO(LOG_Enter("");)
	/* do nothing. */
}

void DOPE_InitOSKeymap(_THIS) {
	INFO(LOG_Enter("");)

	memset(keymap, 0, sizeof(keymap));

	keymap[DOPE_KEY_ESC]        = SDLK_ESCAPE;
	keymap[DOPE_KEY_ENTER]      = SDLK_RETURN;
	keymap[DOPE_KEY_LEFTCTRL]   = SDLK_LCTRL;
	keymap[DOPE_KEY_LEFTSHIFT]  = SDLK_LSHIFT;
	keymap[DOPE_KEY_RIGHTSHIFT] = SDLK_RSHIFT;
	keymap[DOPE_KEY_LEFTALT]    = SDLK_RALT;
	keymap[DOPE_KEY_CAPSLOCK]   = SDLK_CAPSLOCK;
	keymap[DOPE_KEY_Q] = SDLK_q;
	keymap[DOPE_KEY_W] = SDLK_w;
	keymap[DOPE_KEY_E] = SDLK_e;
	keymap[DOPE_KEY_R] = SDLK_r;
	keymap[DOPE_KEY_T] = SDLK_t;
	keymap[DOPE_KEY_Y] = SDLK_y;
	keymap[DOPE_KEY_U] = SDLK_u;
	keymap[DOPE_KEY_I] = SDLK_i;
	keymap[DOPE_KEY_O] = SDLK_o;
	keymap[DOPE_KEY_P] = SDLK_p;
	keymap[DOPE_KEY_A] = SDLK_a;
	keymap[DOPE_KEY_S] = SDLK_s;
	keymap[DOPE_KEY_D] = SDLK_d;
	keymap[DOPE_KEY_F] = SDLK_f;
	keymap[DOPE_KEY_G] = SDLK_g;
	keymap[DOPE_KEY_H] = SDLK_h;
	keymap[DOPE_KEY_J] = SDLK_j;
	keymap[DOPE_KEY_K] = SDLK_k;
	keymap[DOPE_KEY_L] = SDLK_l;
	keymap[DOPE_KEY_Z] = SDLK_z;
	keymap[DOPE_KEY_X] = SDLK_x;
	keymap[DOPE_KEY_C] = SDLK_c;
	keymap[DOPE_KEY_V] = SDLK_v;
	keymap[DOPE_KEY_B] = SDLK_b;
	keymap[DOPE_KEY_N] = SDLK_n;
	keymap[DOPE_KEY_M] = SDLK_m;
	keymap[DOPE_KEY_F1]  = SDLK_F1;
	keymap[DOPE_KEY_F2]  = SDLK_F2;
	keymap[DOPE_KEY_F3]  = SDLK_F3;
	keymap[DOPE_KEY_F4]  = SDLK_F4;
	keymap[DOPE_KEY_F5]  = SDLK_F5;
	keymap[DOPE_KEY_F6]  = SDLK_F6;
	keymap[DOPE_KEY_F7]  = SDLK_F7;
	keymap[DOPE_KEY_F8]  = SDLK_F8;
	keymap[DOPE_KEY_F9]  = SDLK_F9;
	keymap[DOPE_KEY_F10] = SDLK_F10;
	keymap[DOPE_KEY_F11] = SDLK_F11;
	keymap[DOPE_KEY_F12] = SDLK_F12;
	keymap[DOPE_KEY_NUMLOCK]    = SDLK_NUMLOCK;
	keymap[DOPE_KEY_SCROLLLOCK] = SDLK_SCROLLOCK;
	keymap[DOPE_KEY_KP0] = SDLK_KP0;
	keymap[DOPE_KEY_KP1] = SDLK_KP1;
	keymap[DOPE_KEY_KP2] = SDLK_KP2;
	keymap[DOPE_KEY_KP3] = SDLK_KP3;
	keymap[DOPE_KEY_KP4] = SDLK_KP4;
	keymap[DOPE_KEY_KP5] = SDLK_KP5;
	keymap[DOPE_KEY_KP6] = SDLK_KP6;
	keymap[DOPE_KEY_KP7] = SDLK_KP7;
	keymap[DOPE_KEY_KP8] = SDLK_KP8;
	keymap[DOPE_KEY_KP9] = SDLK_KP9;
	keymap[DOPE_KEY_KPMINUS]   = SDLK_KP_MINUS;
	keymap[DOPE_KEY_KPPLUS]    = SDLK_KP_PLUS;
	keymap[DOPE_KEY_KPDOT]     = SDLK_KP_PERIOD;
	keymap[DOPE_KEY_KPENTER]   = SDLK_KP_ENTER;
	keymap[DOPE_KEY_RIGHTCTRL] = SDLK_RCTRL;
	keymap[DOPE_KEY_KPSLASH]   = SDLK_KP_DIVIDE;
	keymap[DOPE_KEY_RIGHTALT]  = SDLK_RALT;
	keymap[DOPE_KEY_HOME]      = SDLK_HOME;
	keymap[DOPE_KEY_UP]        = SDLK_UP;
	keymap[DOPE_KEY_PAGEUP]    = SDLK_PAGEDOWN;
	keymap[DOPE_KEY_LEFT]      = SDLK_LEFT;
	keymap[DOPE_KEY_RIGHT]     = SDLK_RIGHT;
	keymap[DOPE_KEY_END]       = SDLK_END;
	keymap[DOPE_KEY_DOWN]      = SDLK_DOWN;
	keymap[DOPE_KEY_PAGEDOWN]  = SDLK_PAGEUP;
	keymap[DOPE_KEY_INSERT]    = SDLK_INSERT;
	keymap[DOPE_KEY_DELETE]    = SDLK_DELETE;
	keymap[DOPE_KEY_PAUSE]     = SDLK_PAUSE;
	keymap[DOPE_KEY_LEFTMETA]  = SDLK_LSUPER;
	keymap[DOPE_KEY_RIGHTMETA] = SDLK_RSUPER;
	keymap[DOPE_KEY_RIGHTALT]  = SDLK_MODE;
}

static void event_callback(dope_event *e, void *arg) {
	DOPE_CallbackData *cd = (DOPE_CallbackData*) arg;
	_THIS;
	long code;
	Uint8 state;
	SDL_keysym key;

	INFO(LOG_Enter("(obj=\"%s\", type=%lx)", cd->obj, e->type);)
	
	memset(&key, 0, sizeof(key));

	this = cd->this;

	switch (e->type) {
		case EVENT_TYPE_PRESS:
		case EVENT_TYPE_RELEASE:
			if (e->type==EVENT_TYPE_PRESS) {
				state = SDL_PRESSED;
				code = e->press.code;
			} else {
				state = SDL_RELEASED;
				code = e->release.code;
			}
			// set hardware code
			key.scancode = code;
			// look into keymap
			if (code>0 && code < DOPE_KEY_MAX)
				key.sym = keymap[code];
			// or try to decode ascii
			if (!key.sym) {
				if (this->hidden->have_app) {
					key.sym = dope_get_ascii(this->hidden->app_id, code);
					key.unicode = key.sym;
				}
			}
			// catch some cases by hand
			if (!key.sym) {
				switch (code) {
					case DOPE_BTN_LEFT:
					case DOPE_BTN_RIGHT:
					case DOPE_BTN_MIDDLE:
					case DOPE_BTN_SIDE:
					case DOPE_BTN_EXTRA:
					case DOPE_BTN_FORWARD:
					case DOPE_BTN_BACK:
						SDL_PrivateMouseButton(state, code-DOPE_BTN_MOUSE+1, 0, 0);
						break;
					default:
						LOG("(\"%s\") unknown keycode: %li", cd->obj, code);
				}
			}
			if (key.sym) SDL_PrivateKeyboard(state, &key);
			break;
		case EVENT_TYPE_MOTION:
			SDL_PrivateMouseMotion(0, 0, e->motion.abs_x, e->motion.abs_y);
			break;
		case EVENT_TYPE_COMMAND:
// broken: content of e->command.cmd is undefined
// but there was no useful code here anyway
//			INFO(LOG("(\"%s\") EVENT_TYPE_COMMAND: 0x%lx \"%s\"", cd->obj, e->command.cmd, e->command.cmd);)
//			if (strcmp(e->command.cmd, "catch")==0) {
//				// we gained control over the mouse :)
//			} else if (strcmp(e->command.cmd, "discharge")==0) {
//				// we have lost mouse control :(
//			} else if (strcmp(e->command.cmd, "enter")==0) {
//				// mouse is over vscreen
//			} else if (strcmp(e->command.cmd, "leave")==0) {
//				// mouse left vscreen
//			} else {
//				LOG("(\"%s\") unknown command \"%s\"", cd->obj, e->command.cmd);
//			}
			break;
		default:
			LOG("(\"%s\") unknown event type %lx", cd->obj, e->type);
	}
}

static void event_thread(void* data) {
	long app_id = *( (long*)data);

	INFO(LOG_Enter("");)

	l4thread_started(NULL);
	dope_eventloop(app_id);
}

void DOPE_BindEventHandler(_THIS, DOPE_CallbackData *cd) {
	cd->this = this;
	dope_bind(this->hidden->app_id, cd->obj, "press",     event_callback, cd);
	dope_bind(this->hidden->app_id, cd->obj, "release",   event_callback, cd);
	dope_bind(this->hidden->app_id, cd->obj, "motion",    event_callback, cd);
	dope_bind(this->hidden->app_id, cd->obj, "leave",     event_callback, cd);
	dope_bind(this->hidden->app_id, cd->obj, "enter",     event_callback, cd);
	dope_bind(this->hidden->app_id, cd->obj, "catch",     event_callback, cd);
	dope_bind(this->hidden->app_id, cd->obj, "discharge", event_callback, cd);
}

void DOPE_InstallEventHandler(_THIS) {
	INFO(LOG_Enter("");)
	if (this->hidden->have_app) {
		DOPE_BindEventHandler(this, &CD_vscr);
		DOPE_BindEventHandler(this, &CD_win);
		this->hidden->eventthread=l4thread_create(event_thread, &this->hidden->app_id, L4THREAD_CREATE_SYNC);
	}
}

void DOPE_RemoveEventHandler(_THIS) {
	INFO(LOG_Enter("");)
	if (this->hidden->have_app) {
		if (this->hidden->eventthread > 0) {
			if (l4thread_shutdown(this->hidden->eventthread)) {
				LOGl("could not kill event thread");
			}
			this->hidden->eventthread = 0;
		}
	}
}

