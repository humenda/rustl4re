#ifndef _SDL_ovlwmvideo_h
#define _SDL_ovlwmvideo_h

#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_mutex.h"
#include <l4/thread/thread.h>

// logging or not
#if 0
	#define INFO(cmd) cmd SDL_Delay(20);
#else
	#define INFO(cmd)
#endif

#define OVLWMVID_DRIVER_NAME "overlay_wm"
#define MAXTITLE 127
#define WINTITLE "SDL window"
#define APPTITLE "SDL app"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *this


/* Private display data */
struct SDL_PrivateVideoData {
	char have_app;
	long app_id;
	l4thread_t eventthread;
	char* db_realscreen;
};

/* function list */
/* Initialization functions */
int          OVLWM_VideoInit(_THIS, SDL_PixelFormat *vformat);
SDL_Rect   **OVLWM_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
void         OVLWM_VideoQuit(_THIS);
/* Worker functions */
SDL_Surface *OVLWM_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
int          OVLWM_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
void         OVLWM_UpdateRects(_THIS, int numrects, SDL_Rect *rects);
/* Hardware surface functions */
int          OVLWM_FlipHWSurface(_THIS, SDL_Surface *surface);
int          OVLWM_AllocHWSurface(_THIS, SDL_Surface *surface);
int          OVLWM_LockHWSurface(_THIS, SDL_Surface *surface);
void         OVLWM_UnlockHWSurface(_THIS, SDL_Surface *surface);
void         OVLWM_FreeHWSurface(_THIS, SDL_Surface *surface);
/* window manager functions */
void         OVLWM_SetCaption(_THIS, const char *title, const char *icon);
SDL_GrabMode OVLWM_GrabInput(_THIS, SDL_GrabMode mode);
void         OVLWM_WarpWMCursor(_THIS, Uint16 x, Uint16 y);
/* event functions */
void         OVLWM_PumpEvents(_THIS);
void         OVLWM_InitOSKeymap(_THIS);
/* private funs */
void         OVLWM_RemoveApp(_THIS);
void         OVLWM_InstallEventHandler(_THIS);
void         OVLWM_RemoveEventHandler(_THIS);
#endif /* _SDL_ovlwmvideo_h */
