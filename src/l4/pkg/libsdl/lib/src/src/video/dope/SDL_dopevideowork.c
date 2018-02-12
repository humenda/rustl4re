#include <string.h>
#include <malloc.h>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"

#include "SDL_dopevideo.h"

#include <l4/log/l4log.h>
#include <l4/dope/dopelib.h>
#include <l4/dope/vscreen.h>

// atoi taken from dietlibc
// included to avoid library dependencies 
static inline int atoi(const char* s) {
  long int v=0;
  int sign=1;
  while ( *s == ' '  ||  (unsigned int)(*s - 9) < 5u) s++;
  switch (*s) {
  case '-': sign=-1;
  case '+': ++s;
  }
  while ((unsigned int) (*s - '0') < 10u) {
    v=v*10+*s-'0'; ++s;
  }
  return sign==-1?-v:v;
}


double DOPE_video_scale_factor __attribute__ ((weak)) = 1.0;

void FreeAndNull(void** ptr) {
	if (ptr) {
		if (*ptr) {
			free(*ptr);
			*ptr = NULL;
		}
	}
}

SDL_Surface *DOPE_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags) {
	char* apptitle = APPTITLE;
	char* wintitle = WINTITLE;
	int scr_wx, scr_width, scr_wy, scr_height;
	char req_buf[16];  // receive buffer for dope_req
 
	INFO(LOG_Enter("(width=%i, height=%i, bpp=%i, flags=%8x)", width, height, bpp, flags);)

	// make sure to have correct scale
	if (DOPE_video_scale_factor<=0) DOPE_video_scale_factor = 1.0;
	
	// lose dope window(-s)
	DOPE_RemoveApp(this);

	// reject unsupported bpp
	if (bpp != 16) return NULL;
	// reject unsupported flags
	if (flags & SDL_OPENGL) return NULL;

	/* Allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, 0, 0, 0, 0) ) {
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}

	// if we were double buffered clear shadow buffer
	if (current->flags & SDL_DOUBLEBUF) {
		FreeAndNull(current->pixels);
	}

	// init new window
	if (this->wm_title) wintitle = this->wm_title;
	if (this->wm_icon)  apptitle = this->wm_icon;

	// register dope app
	this->hidden->app_id = dope_init_app(apptitle);

	// get screen resolution to center window
	dope_req (this->hidden->app_id, req_buf, 16, "screen.w");
        scr_width = atoi (req_buf);
	dope_req (this->hidden->app_id, req_buf, 16, "screen.h");
        scr_height = atoi (req_buf);
        // if we get wrong screen size we take 800x600 as default 
	if ((scr_height <= 0) || (scr_width <= 0))
	{
    	    scr_width = 800;
    	    scr_height = 600;
	}
	// calculate postion for center
	scr_wx = scr_width/2 - ((int) (width*DOPE_video_scale_factor)/2);
	scr_wy = scr_height/2 - ((int) (height*DOPE_video_scale_factor)/2);

	// openwindow
	dope_cmdf(this->hidden->app_id, "win=new Window()");
	dope_cmdf(this->hidden->app_id, "win.set(-workx %i -worky %i -workw %i -workh %i)", scr_wx, scr_wy, (int) (width*DOPE_video_scale_factor), (int) (height*DOPE_video_scale_factor));
	dope_cmdf(this->hidden->app_id, "win.set(-background off)");
	dope_cmdf(this->hidden->app_id, "vscr=new VScreen(-grabmouse yes)");
	dope_cmdf(this->hidden->app_id, "vscr.setmode(%i, %i, \"%s\")", width, height, "RGB16");
	dope_cmdf(this->hidden->app_id, "win.set(-content vscr)");
	this->hidden->have_app = 1;

	DOPE_SetCaption(this, wintitle, apptitle);

	DOPE_InstallEventHandler(this);

	/* Set up the new mode framebuffer */
	current->w = width;
	current->h = height;
	current->pitch = current->w * (bpp / 8);
	current->flags = SDL_RESIZABLE | SDL_PREALLOC | SDL_ASYNCBLIT;
	current->pixels = vscr_get_fb(this->hidden->app_id, "vscr");
	memset(current->pixels, 0, current->h*current->pitch);

	if (flags & SDL_DOUBLEBUF) {
		// if we don't set SDL_DOUBLEBUF UpdateRect will be called instead of
		// FlipHWSurface, but DOpE doesn't repaint immediately on vscr.refresh
		// so we emulate double buffering to give dope time to repaint

		// try to alloc shadow buffer
		this->hidden->db_realscreen = current->pixels;
		current->pixels = malloc(current->h*current->pitch);
		if (current->pixels) {
			// success!
			current->flags |= SDL_DOUBLEBUF|SDL_HWSURFACE;
			memset(current->pixels, 0, current->h*current->pitch);
		} else {
			// failed -> return real screen
			current->pixels = this->hidden->db_realscreen;
		}
	}

	// finally open the window
	dope_cmdf(this->hidden->app_id, "win.open()");

	/* We're done */
	return(current);
}

int DOPE_ToggleFullScreen(_THIS, int on) {
	
	// remember window size for fullscreen toggle
	static int work_x, work_y ,work_w, work_h;
	static int is_fs = 0;
	
	int scr_width, scr_height;
	char req_buf[16];  // receive buffer for dope_req

	if((on)&&(!is_fs))
	{
	    //toggle fullscreen on
	    is_fs = 1;

	    // get position and size of current window
    	    dope_req (this->hidden->app_id, req_buf, 16, "win.workx");
    	    work_x = atoi (req_buf);
	    dope_req (this->hidden->app_id, req_buf, 16, "win.worky");
    	    work_y = atoi (req_buf);
    	    dope_req (this->hidden->app_id, req_buf, 16, "win.workw");
    	    work_w = atoi (req_buf);
	    dope_req (this->hidden->app_id, req_buf, 16, "win.workh");
    	    work_h = atoi (req_buf);

	    // get screen resolution to center window
    	    dope_req (this->hidden->app_id, req_buf, 16, "screen.w");
    	    scr_width = atoi (req_buf);
	    dope_req (this->hidden->app_id, req_buf, 16, "screen.h");
    	    scr_height = atoi (req_buf);
    	    // if we get wrong screen size we take 800x600 as default 
	    if ((scr_height <= 0) || (scr_width <= 0))
	    {
    		scr_width = 800;
    		scr_height = 600;
	    }
	    // set window size
	    dope_cmdf(this->hidden->app_id, "win.set(-workx 0 -worky 0 -workw %i -workh %i)", scr_width, scr_height);
	}
	else if((!on)&&(is_fs))
	{
	    //toggle fullscreen off 
	    is_fs = 0;
	    // set window size
    	    dope_cmdf(this->hidden->app_id, "win.set(-workx %i -worky %i -workw %i -workh %i)", work_x, work_y, work_w, work_h);
	}
	
	return(0);
}

int DOPE_AllocHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return(-1);
}
void DOPE_FreeHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return;
}

int DOPE_LockHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return(0);
}
void DOPE_UnlockHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return;
}

void DOPE_UpdateRects(_THIS, int numrects, SDL_Rect *rects) {
	int i;
	INFO(LOG_Enter("(numrects=%i)", numrects);)
	if (this->hidden->have_app) {
		for (i=0; i<numrects; i++) {
			INFO(LOG("vscr.refresh(x=%i, y=%i, w=%i, h=%i)", rects[i].x, rects[i].y, rects[i].w, rects[i].h);)
			dope_cmdf(this->hidden->app_id, "vscr.refresh(-x %i -y %i -w %i -h %i)", rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		}
	}
}

int DOPE_FlipHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	// copy buffer and update screen
	memcpy(this->hidden->db_realscreen, surface->pixels, surface->h*surface->pitch);
	if (this->hidden->have_app)
		dope_cmdf(this->hidden->app_id, "vscr.refresh()");
	return 0;
}

int DOPE_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors) {
	INFO(LOG_Enter("(ncolors=%i)", ncolors);)
	/* return success */
	return(1);
}

void DOPE_WarpWMCursor(_THIS, Uint16 x, Uint16 y) {
	INFO(LOG_Enter("(x=%i, y=%i)", x, y);)

	if (this->hidden->have_app) {
		dope_cmdf(this->hidden->app_id, "vscr.set(-mousex %i -mousey %i)", x, y);
	}
}

SDL_GrabMode DOPE_GrabInput(_THIS, SDL_GrabMode mode) {
	INFO(LOG_Enter("(mode=%i)", mode);)
	if (this->hidden->have_app) {
		dope_cmdf(this->hidden->app_id, "vscr.set(-grabmouse %s)", (mode==SDL_GRAB_OFF?"no":"yes"));
	}
	return mode;
}
