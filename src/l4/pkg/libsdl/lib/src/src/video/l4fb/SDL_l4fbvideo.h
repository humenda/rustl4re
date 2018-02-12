#ifndef _SDL_dopevideo_h
#define _SDL_dopevideo_h


#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_mutex.h"
#include <pthread.h>

// logging or not
#if 0
	#define INFO(cmd) cmd SDL_Delay(20);
#else
	#define INFO(cmd)
#endif

#define L4FBVID_DRIVER_NAME "dope"
#define MAXTITLE 127
#define WINTITLE "SDL window"
#define APPTITLE "SDL app"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *this

/* function list */
/* Initialization functions */
int          L4FB_VideoInit(_THIS, SDL_PixelFormat *vformat);
SDL_Rect   **L4FB_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
void         L4FB_VideoQuit(_THIS);
/* Worker functions */
SDL_Surface *L4FB_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
int          L4FB_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
void         L4FB_UpdateRects(_THIS, int numrects, SDL_Rect *rects);
int          L4FB_ToggleFullScreen(_THIS, int on);
/* Hardware surface functions */
int          L4FB_FlipHWSurface(_THIS, SDL_Surface *surface);
int          L4FB_AllocHWSurface(_THIS, SDL_Surface *surface);
int          L4FB_LockHWSurface(_THIS, SDL_Surface *surface);
void         L4FB_UnlockHWSurface(_THIS, SDL_Surface *surface);
void         L4FB_FreeHWSurface(_THIS, SDL_Surface *surface);
/* window manager functions */
void         L4FB_SetCaption(_THIS, const char *title, const char *icon);
SDL_GrabMode L4FB_GrabInput(_THIS, SDL_GrabMode mode);
void         L4FB_WarpWMCursor(_THIS, Uint16 x, Uint16 y);
/* event functions */
void         L4FB_PumpEvents(_THIS);
void         L4FB_InitOSKeymap(_THIS);
/* private funs */
void         L4FB_RemoveApp(_THIS);
void         L4FB_InstallEventHandler(_THIS);
void         L4FB_RemoveEventHandler(_THIS);

/* Private display data */

#include <l4/re/c/event_buffer.h>
#include <l4/re/c/dataspace.h>
#include <l4/re/c/util/video/goos_fb.h>


struct SDL_PrivateVideoData {
	

	unsigned x_res;               // resolution in SDL
	unsigned y_res;               // (not the resolution of the fb!)


	unsigned x_offset;
	unsigned y_offset;
	unsigned bpp;
	unsigned pitch;

	void * pixels;
	void * fb_start;

	/* the fb stuff */
	l4re_util_video_goos_fb_t goosfb;
	void *fbmem_vaddr;
	l4re_video_view_info_t vvi;

	/* the event stuff */
	l4_cap_idx_t ev_ds;
	l4_cap_idx_t ev_irq;
	l4re_event_buffer_consumer_t ev_buf;
	pthread_t ev_thread;
};


#endif /* _SDL_dopevideo_h */
