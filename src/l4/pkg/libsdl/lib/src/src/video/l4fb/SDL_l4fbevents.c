#include "SDL.h"
#include "SDL_sysevents.h"
#include "SDL_events_c.h"
#include "SDL_l4fbvideo.h"
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
#include <l4/sys/icu.h>

#include <l4/re/c/dataspace.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/event.h>
#include <l4/re/c/event_buffer.h>
#include <l4/re/event_enums.h>
#include <l4/util/thread.h>

#include <pthread-l4.h>
#include <string.h>
#include <stdio.h>


static SDL_VideoDevice *dev;


static SDLKey keymap[L4RE_KEY_MAX];



void L4FB_PumpEvents(_THIS) {
        (void)this;
	INFO(LOG_Enter("");)
	/* do nothing. */
}

void L4FB_InitOSKeymap(_THIS) {
        (void)this;
	INFO(LOG_Enter("");)

	memset(keymap, 0, sizeof(keymap));

	keymap[L4RE_KEY_ESC]        = SDLK_ESCAPE;
	keymap[L4RE_KEY_ENTER]      = SDLK_RETURN;
	keymap[L4RE_KEY_LEFTCTRL]   = SDLK_LCTRL;
	keymap[L4RE_KEY_LEFTSHIFT]  = SDLK_LSHIFT;
	keymap[L4RE_KEY_RIGHTSHIFT] = SDLK_RSHIFT;
	keymap[L4RE_KEY_LEFTALT]    = SDLK_RALT;
	keymap[L4RE_KEY_CAPSLOCK]   = SDLK_CAPSLOCK;
	keymap[L4RE_KEY_Q] = SDLK_q;
	keymap[L4RE_KEY_W] = SDLK_w;
	keymap[L4RE_KEY_E] = SDLK_e;
	keymap[L4RE_KEY_R] = SDLK_r;
	keymap[L4RE_KEY_T] = SDLK_t;
	keymap[L4RE_KEY_Y] = SDLK_y;
	keymap[L4RE_KEY_U] = SDLK_u;
	keymap[L4RE_KEY_I] = SDLK_i;
	keymap[L4RE_KEY_O] = SDLK_o;
	keymap[L4RE_KEY_P] = SDLK_p;
	keymap[L4RE_KEY_A] = SDLK_a;
	keymap[L4RE_KEY_S] = SDLK_s;
	keymap[L4RE_KEY_D] = SDLK_d;
	keymap[L4RE_KEY_F] = SDLK_f;
	keymap[L4RE_KEY_G] = SDLK_g;
	keymap[L4RE_KEY_H] = SDLK_h;
	keymap[L4RE_KEY_J] = SDLK_j;
	keymap[L4RE_KEY_K] = SDLK_k;
	keymap[L4RE_KEY_L] = SDLK_l;
	keymap[L4RE_KEY_Z] = SDLK_z;
	keymap[L4RE_KEY_X] = SDLK_x;
	keymap[L4RE_KEY_C] = SDLK_c;
	keymap[L4RE_KEY_V] = SDLK_v;
	keymap[L4RE_KEY_B] = SDLK_b;
	keymap[L4RE_KEY_N] = SDLK_n;
	keymap[L4RE_KEY_M] = SDLK_m;
	keymap[L4RE_KEY_F1]  = SDLK_F1;
	keymap[L4RE_KEY_F2]  = SDLK_F2;
	keymap[L4RE_KEY_F3]  = SDLK_F3;
	keymap[L4RE_KEY_F4]  = SDLK_F4;
	keymap[L4RE_KEY_F5]  = SDLK_F5;
	keymap[L4RE_KEY_F6]  = SDLK_F6;
	keymap[L4RE_KEY_F7]  = SDLK_F7;
	keymap[L4RE_KEY_F8]  = SDLK_F8;
	keymap[L4RE_KEY_F9]  = SDLK_F9;
	keymap[L4RE_KEY_F10] = SDLK_F10;
	keymap[L4RE_KEY_F11] = SDLK_F11;
	keymap[L4RE_KEY_F12] = SDLK_F12;
	keymap[L4RE_KEY_NUMLOCK]    = SDLK_NUMLOCK;
	keymap[L4RE_KEY_SCROLLLOCK] = SDLK_SCROLLOCK;
	keymap[L4RE_KEY_KP0] = SDLK_KP0;
	keymap[L4RE_KEY_KP1] = SDLK_KP1;
	keymap[L4RE_KEY_KP2] = SDLK_KP2;
	keymap[L4RE_KEY_KP3] = SDLK_KP3;
	keymap[L4RE_KEY_KP4] = SDLK_KP4;
	keymap[L4RE_KEY_KP5] = SDLK_KP5;
	keymap[L4RE_KEY_KP6] = SDLK_KP6;
	keymap[L4RE_KEY_KP7] = SDLK_KP7;
	keymap[L4RE_KEY_KP8] = SDLK_KP8;
	keymap[L4RE_KEY_KP9] = SDLK_KP9;
	keymap[L4RE_KEY_KPMINUS]   = SDLK_KP_MINUS;
	keymap[L4RE_KEY_KPPLUS]    = SDLK_KP_PLUS;
	keymap[L4RE_KEY_KPDOT]     = SDLK_KP_PERIOD;
	keymap[L4RE_KEY_KPENTER]   = SDLK_KP_ENTER;
	keymap[L4RE_KEY_RIGHTCTRL] = SDLK_RCTRL;
	keymap[L4RE_KEY_KPSLASH]   = SDLK_KP_DIVIDE;
	keymap[L4RE_KEY_RIGHTALT]  = SDLK_RALT;
	keymap[L4RE_KEY_HOME]      = SDLK_HOME;
	keymap[L4RE_KEY_UP]        = SDLK_UP;
	keymap[L4RE_KEY_PAGEUP]    = SDLK_PAGEDOWN;
	keymap[L4RE_KEY_LEFT]      = SDLK_LEFT;
	keymap[L4RE_KEY_RIGHT]     = SDLK_RIGHT;
	keymap[L4RE_KEY_END]       = SDLK_END;
	keymap[L4RE_KEY_DOWN]      = SDLK_DOWN;
	keymap[L4RE_KEY_PAGEDOWN]  = SDLK_PAGEUP;
	keymap[L4RE_KEY_INSERT]    = SDLK_INSERT;
	keymap[L4RE_KEY_DELETE]    = SDLK_DELETE;
	keymap[L4RE_KEY_PAUSE]     = SDLK_PAUSE;
	keymap[L4RE_KEY_LEFTMETA]  = SDLK_LSUPER;
	keymap[L4RE_KEY_RIGHTMETA] = SDLK_RSUPER;
	keymap[L4RE_KEY_RIGHTALT]  = SDLK_MODE;
        keymap[L4RE_KEY_SPACE]     = SDLK_SPACE;
}


L4_CV
static void event_callback(l4re_event_t *e, void *data) {
	long code;
	Uint8 state;
	SDL_keysym key;

	(void)data;
	memset(&key, 0, sizeof(key));
	switch (e->type) {
		case L4RE_EV_KEY:
			/* pressed or released? */
                        state = e->value ? SDL_PRESSED : SDL_RELEASED;
			code = e->code;
			// set hardware code
			key.scancode = e->code;

			// look into keymap
			if (code>0 && code < L4RE_KEY_MAX)
				key.sym = keymap[code];
			// or try to decode ascii
#if 0
			if (!key.sym) {
				if (this->hidden->have_app) {
					key.sym = dope_get_ascii(this->hidden->app_id, code);
					key.unicode = key.sym;
				}
			}
#endif

			// catch some cases by hand
			if (!key.sym) {
				switch (code) {
					case L4RE_BTN_LEFT:
					case L4RE_BTN_RIGHT:
					case L4RE_BTN_MIDDLE:
					case L4RE_BTN_SIDE:
					case L4RE_BTN_EXTRA:
					case L4RE_BTN_FORWARD:
					case L4RE_BTN_BACK:
						SDL_PrivateMouseButton(state, code-L4RE_BTN_MOUSE+1, 0, 0);
						break;
					default:
						printf("unknown keycode: %li\n", code);
				}
			}
			if (key.sym)
				SDL_PrivateKeyboard(state, &key);
			break;
		case L4RE_EV_ABS:
			{
				/* because we only get x OR y pos we have to remember the last
				 * y OR x value ... */
				static Sint16 my_x, my_y;

				switch (e->code) {

					case L4RE_ABS_X:
						my_x = e->value - dev->hidden->x_offset;
						break;
					case L4RE_ABS_Y:
						my_y = e->value- dev->hidden->y_offset;
						break;
				}
				SDL_PrivateMouseMotion(0, 0, my_x, my_y);
			}
			break;
		case L4RE_EV_REL:
			{
				switch (e->code)
					{
						case L4RE_REL_X:
							SDL_PrivateMouseMotion(0, 1, e->value, 0);
							break;
						case L4RE_REL_Y:
							SDL_PrivateMouseMotion(0, 1, 0, e->value);
					}
				break;
			}
                case L4RE_EV_SYN:
                        {
                          // ignore SYNs
                          break;
                        }


//		case L4RE_EVENT_TYPE_COMMAND:
// broken: content of e->command.cmd is undefined
// but there was no useful code here anyway
//			INFO(LOG("(\"%s\") L4RE_EVENT_TYPE_COMMAND: 0x%lx \"%s\"", cd->obj, e->command.cmd, e->command.cmd);)
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
//			break;
		default:
			printf("unknown event type %x\n", e->type);
	}
}


static pthread_mutex_t ev_init_wait = PTHREAD_MUTEX_INITIALIZER;

static void * ev_loop(void * data)
{
	SDL_VideoDevice * d = (SDL_VideoDevice *) data;

	printf("Input handler thread started\n");

        pthread_mutex_lock(&ev_init_wait);
        pthread_mutex_unlock(&ev_init_wait);

	l4re_event_buffer_consumer_process(&d->hidden->ev_buf,
	                                   d->hidden->ev_irq,
	                                   pthread_l4_cap(d->hidden->ev_thread),
	                                   NULL, event_callback);
	printf("Input handler terminates\n");
	return 0;
}




void L4FB_InstallEventHandler(_THIS)
{
  INFO(LOG_Enter("");)
  int ret;

  L4FB_InitOSKeymap(this);

  if (l4_is_invalid_cap(this->hidden->ev_ds = l4re_util_cap_alloc()))
    {
      /* TODO: handle ERROR */
      printf("ERROR: ev_ds cap_alloc\n");
      return;
    }


  if (l4_is_invalid_cap(this->hidden->ev_irq = l4re_util_cap_alloc()))
    {
      /* TODO: handle ERROR */
      printf("ERROR: ev_irq cap_alloc\n");
      return;
    }

  if ((ret = l4_error(l4_factory_create_irq(l4re_env()->factory, this->hidden->ev_irq))))
    {
      /* TODO: handle ERROR */
      printf("ERROR: Creating irq: %d \n", ret);
      return;
    }
  if ((ret = l4_error(l4_icu_bind(l4re_util_video_goos_fb_goos(&this->hidden->goosfb),
            0, this->hidden->ev_irq))))
    {
      /* TODO: handle ERROR */
      printf("ERROR: Creating irq: %d \n", ret);
      return;
    }
  if ((ret = l4re_event_get_buffer(l4re_util_video_goos_fb_goos(&this->hidden->goosfb),
          this->hidden->ev_ds)))
    {
      /* TODO: handle ERROR */
      printf("ERROR: l4re_event_get_buffer: %d \n", ret);
      return;
    }

  if (l4re_event_buffer_attach(&this->hidden->ev_buf, this->hidden->ev_ds, l4re_env()->rm))
    {
      /* TODO: handle ERROR */
      printf("ERROR: l4re_buffer_attach \n");
      return;
    }


  dev = this;
  /* ev_loop needs to wait until pthread_create returns so that
   * this->hidden->ev_thread is set, as it uses it */
  pthread_mutex_lock(&ev_init_wait);
  if (pthread_create(&this->hidden->ev_thread, NULL, ev_loop, (void*)this))
    /* TODO: handle ERROR */
    printf("ERROR: ev_thread create \n");
  pthread_mutex_unlock(&ev_init_wait);
}

void L4FB_RemoveEventHandler(_THIS) {
        (void)this;

	/* TODO: Stop event thread */
}

