#ifndef _SDL_dopevideo_h
#define _SDL_dopevideo_h

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

#define DOPEVID_DRIVER_NAME "dope"
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
int          DOPE_VideoInit(_THIS, SDL_PixelFormat *vformat);
SDL_Rect   **DOPE_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
void         DOPE_VideoQuit(_THIS);
/* Worker functions */
SDL_Surface *DOPE_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
int          DOPE_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
void         DOPE_UpdateRects(_THIS, int numrects, SDL_Rect *rects);
int          DOPE_ToggleFullScreen(_THIS, int on);
/* Hardware surface functions */
int          DOPE_FlipHWSurface(_THIS, SDL_Surface *surface);
int          DOPE_AllocHWSurface(_THIS, SDL_Surface *surface);
int          DOPE_LockHWSurface(_THIS, SDL_Surface *surface);
void         DOPE_UnlockHWSurface(_THIS, SDL_Surface *surface);
void         DOPE_FreeHWSurface(_THIS, SDL_Surface *surface);
/* window manager functions */
void         DOPE_SetCaption(_THIS, const char *title, const char *icon);
SDL_GrabMode DOPE_GrabInput(_THIS, SDL_GrabMode mode);
void         DOPE_WarpWMCursor(_THIS, Uint16 x, Uint16 y);
/* event functions */
void         DOPE_PumpEvents(_THIS);
void         DOPE_InitOSKeymap(_THIS);
/* private funs */
void         DOPE_RemoveApp(_THIS);
void         DOPE_InstallEventHandler(_THIS);
void         DOPE_RemoveEventHandler(_THIS);
#endif /* _SDL_dopevideo_h */
