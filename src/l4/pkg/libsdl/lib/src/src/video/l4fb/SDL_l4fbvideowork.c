#include <string.h>
#include <malloc.h>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"

#include "SDL_l4fbvideo.h"

#include <stdio.h>

double L4FB_video_scale_factor __attribute__ ((weak)) = 1.0;

static void FreeAndNull(void** ptr) {
	if (ptr) {
		if (*ptr) {
			free(*ptr);
			*ptr = NULL;
		}
	}
}

SDL_Surface *L4FB_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags) {
	char* apptitle = APPTITLE;
	char* wintitle = WINTITLE;

	printf("L4FB_SetVideoMode: %dx%d@%d:\n", width, height, bpp);
	/*
	 * is requested resolution higher than the framebuffer's one?
	 */

	if( width > (int)this->hidden->vvi.width
		|| height > (int)this->hidden->vvi.height)
	{
		return (NULL);
	}
	
	/*
	 * calc the offsets for current resolution the center the image 
	 */
	
	this->hidden->x_offset = (this->hidden->vvi.width - width ) / 2;
	this->hidden->y_offset = (this->hidden->vvi.height - height) / 2;

	// make sure to have correct scale
	if (L4FB_video_scale_factor<=0) L4FB_video_scale_factor = 1.0;
	
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



//	L4FB_SetCaption(this, wintitle, apptitle);

	if (l4_is_invalid_cap(this->hidden->ev_ds) || !this->hidden->ev_ds)
		L4FB_InstallEventHandler(this);

	/* Set up the new mode framebuffer */
	current->w = width;
	current->h = height;
	current->pitch = SDL_CalculatePitch(current);
	this->hidden->pitch =  current->pitch;
	current->flags =  SDL_PREALLOC | SDL_ASYNCBLIT;


//	if (current->pixels)
//	{
//		/* TODO: ERROR HANDLING */
//	}

//	memset(current->pixels, 0, current->h*current->pitch);


	/* since we want to center image in fb we allway have to use doublebuf... */

	this->hidden->fb_start = this->hidden->fbmem_vaddr + this->hidden->vvi.buffer_offset;

	current->pixels = malloc(current->h*current->pitch);

	if (current->pixels) {
		// success!
		printf("allocated shadow buffer\n");
		this->hidden->pixels = current->pixels;
		memset(current->pixels, 0, current->h*current->pitch);
	} else
		return NULL;


	if (flags&SDL_DOUBLEBUF) {

		current->flags |= SDL_DOUBLEBUF|SDL_HWSURFACE;
		// try to alloc shadow buffer
	}
 
	/* We're done */
	return(current);
}

int L4FB_ToggleFullScreen(_THIS, int on) {
	(void)this;
	/* TODO: NOT IMPLENETED */

	// remember window size for fullscreen toggle
	
	static int is_fs = 0;
	

	if((on)&&(!is_fs))
	{
		/* TODO: set fs */
	}
	else if((!on)&&(is_fs))
	{
		/* TODO: unset FS */
	}
	
	return(0);
}

int L4FB_AllocHWSurface(_THIS, SDL_Surface *surface) {
	(void)this;
	(void)surface;
	//INFO(LOG_Enter("");)
	return(-1);
}
void L4FB_FreeHWSurface(_THIS, SDL_Surface *surface) {
	(void)this;
	(void)surface;
	//INFO(LOG_Enter("");)
	return;
}

int L4FB_LockHWSurface(_THIS, SDL_Surface *surface) {
	(void)this;
	(void)surface;
//	INFO(LOG_Enter("");)
	return(0);
}
void L4FB_UnlockHWSurface(_THIS, SDL_Surface *surface) {
	(void)this;
	(void)surface;
//	INFO(LOG_Enter("");)
	return;
}

void L4FB_UpdateRects(_THIS, int numrects, SDL_Rect *rects) {
	int i;

	l4re_video_view_info_t * vvi = &this->hidden->vvi;

	for (i=0; i<numrects; i++) {
		int j;
		for (j=0; j<rects[i].h; j++) {
			void *dest = this->hidden->fb_start
			             + vvi->bytes_per_line
						   *(this->hidden->y_offset+rects[i].y+j)
						+ this->hidden->x_offset*vvi->pixel_info.bytes_per_pixel
						+ rects[i].x*vvi->pixel_info.bytes_per_pixel;

			void *src = this->hidden->pixels
		                + (this->hidden->pitch
					       * (j+rects[i].y))
		                + rects[i].x * vvi->pixel_info.bytes_per_pixel;

			unsigned len = rects[i].w * vvi->pixel_info.bytes_per_pixel;
			memcpy(dest,src,len);
		}

		l4re_util_video_goos_fb_refresh(&this->hidden->goosfb, rects[i].x+this->hidden->x_offset, rects[i].y+this->hidden->y_offset, rects[i].w, rects[i].h);
	}
}

int L4FB_FlipHWSurface(_THIS, SDL_Surface *surface) {
	l4re_video_view_info_t * vvi = &this->hidden->vvi;

	// copy buffer and update screen
	int i;
	/*
	 * we have to do it linewise because real screen size and surface size my differ
	 */
	for(i=0; i<surface->h; i++) {
		/* calc dest addr */
		void *dest = this->hidden->fb_start
		             + vvi->bytes_per_line
					   * (this->hidden->y_offset+i)
					 + this->hidden->x_offset * vvi->pixel_info.bytes_per_pixel; 

		memcpy(dest, surface->pixels+(surface->pitch * i), surface->pitch);
	}

	l4re_util_video_goos_fb_refresh(&this->hidden->goosfb, this->hidden->x_offset, this->hidden->y_offset, surface->w, surface->h);
//	printf("refresh\n");
	return 0;
}

int L4FB_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors) {
	(void)this;
	(void)firstcolor;
	(void)ncolors;
	(void)colors;
	/* return success */
	return(1);
}

void L4FB_WarpWMCursor(_THIS, Uint16 x, Uint16 y) {
	(void)this;
	(void)x;
	(void)y;
	/* TODO: not implemented */
}

SDL_GrabMode L4FB_GrabInput(_THIS, SDL_GrabMode mode) {
	(void)this;
		//(mode==SDL_GRAB_OFF?"no":"yes"));
		/*TODO:  not implemented */
	return mode;
}
