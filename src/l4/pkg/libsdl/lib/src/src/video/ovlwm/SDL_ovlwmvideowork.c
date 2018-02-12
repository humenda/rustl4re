#include <string.h>
#include <malloc.h>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"

#include "SDL_ovlwmvideo.h"

#include <l4/log/l4log.h>
#include <l4/overlay_wm/ovl_screen.h>


double OVLWM_video_scale_factor __attribute__ ((weak)) = 1.0;


static void FreeAndNull(void** ptr) {
	if (ptr) {
		if (*ptr) {
			free(*ptr);
			*ptr = NULL;
		}
	}
}


SDL_Surface *OVLWM_SetVideoMode(_THIS, SDL_Surface *current,	int width, int height, int bpp, Uint32 flags) {
	char* apptitle = APPTITLE;
	char* wintitle = WINTITLE;
	INFO(LOG_Enter("(width=%i, height=%i, bpp=%i, flags=%8x)", width, height, bpp, flags);)

	/* make sure to have correct scale */
	if (OVLWM_video_scale_factor<=0) OVLWM_video_scale_factor = 1.0;
	
	OVLWM_RemoveApp(this);

	/* reject unsupported bpp */
	if (bpp != 16) return NULL;
	
	/* reject unsupported flags */
	if (flags & SDL_OPENGL) return NULL;

	/* allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, 0, 0, 0, 0) ) {
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}

	/* if we were double buffered clear shadow buffer */
	if (current->flags & SDL_DOUBLEBUF) {
		FreeAndNull(current->pixels);
	}

	/* init new window */
	if (this->wm_title) wintitle = this->wm_title;
	if (this->wm_icon)  apptitle = this->wm_icon;

	/* open overlay screen */
	ovl_screen_open(width, height, 16);
	this->hidden->have_app = 1;

	OVLWM_InstallEventHandler(this);

	/* set up the new mode framebuffer */
	current->w = width;
	current->h = height;
	current->pitch = current->w * (bpp / 8);
	current->flags = SDL_RESIZABLE | SDL_PREALLOC | SDL_ASYNCBLIT;
	current->pixels = ovl_screen_map();
	memset(current->pixels, 0, current->h*current->pitch);

	if (flags & SDL_DOUBLEBUF) {
		/*
		 * if we don't set SDL_DOUBLEBUF UpdateRect will be called instead of
		 * FlipHWSurface, but DOpE doesn't repaint immediately on vscr.refresh
		 * so we emulate double buffering to give dope time to repaint
		 */

		/* try to alloc shadow buffer */
		this->hidden->db_realscreen = current->pixels;
		current->pixels = malloc(current->h*current->pitch);
		if (current->pixels) {
			
			/* success! */
			current->flags |= SDL_DOUBLEBUF|SDL_HWSURFACE;
			memset(current->pixels, 0, current->h*current->pitch);
		} else {
			
			/* failed -> return real screen */
			current->pixels = this->hidden->db_realscreen;
		}
	}
	
	/* we're done */
	return(current);
}


int OVLWM_AllocHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return(-1);
}


void OVLWM_FreeHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return;
}


int OVLWM_LockHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return(0);
}


void OVLWM_UnlockHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)
	return;
}


void OVLWM_UpdateRects(_THIS, int numrects, SDL_Rect *rects) {
	int i;
	INFO(LOG_Enter("(numrects=%i)", numrects);)
	if (this->hidden->have_app) {
		for (i=0; i<numrects; i++) {
			INFO(LOG("vscr.refresh(x=%i, y=%i, w=%i, h=%i)", rects[i].x, rects[i].y, rects[i].w, rects[i].h);)
			ovl_screen_refresh(rects[i].x, rects[i].y, rects[i].w, rects[i].h);
		}
	}
}


int OVLWM_FlipHWSurface(_THIS, SDL_Surface *surface) {
	INFO(LOG_Enter("");)

	/* copy buffer and update screen */
	memcpy(this->hidden->db_realscreen, surface->pixels, surface->h*surface->pitch);
	if (this->hidden->have_app)
		ovl_screen_refresh(0, 0, surface->w, surface->h);
	return 0;
}


int OVLWM_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors) {
	INFO(LOG_Enter("(ncolors=%i)", ncolors);)
	/* return success */
	return(1);
}


void OVLWM_WarpWMCursor(_THIS, Uint16 x, Uint16 y) {
}


SDL_GrabMode OVLWM_GrabInput(_THIS, SDL_GrabMode mode) {
	INFO(LOG_Enter("(mode=%i)", mode);)
	return mode;
}
