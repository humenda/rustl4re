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


static int OVLWM_Available(void) {
	INFO(LOG_Enter("");)

	/* try to init ovlwm */
	if  (ovl_screen_init(NULL) == 0) return(1);
	return(0);
}


static void OVLWM_DeleteDevice(SDL_VideoDevice *device) {
	INFO(LOG_Enter("");)
	free(device->hidden);
	free(device);
}


static SDL_VideoDevice *OVLWM_CreateDevice(int devindex) {
	SDL_VideoDevice *device;
	INFO(LOG_Enter("(devindex=%i)", devindex);)

	/* contact overlay wm */
	if  (ovl_screen_init(NULL) != 0) return NULL;
	
	/* initialize device */
	(void*) device = malloc(sizeof *device);
	if (device) {
		memset(device, 0, sizeof *device);
		(void*) device->hidden = malloc(sizeof *device->hidden);
		if (device->hidden)
			memset(device->hidden, 0, sizeof *device->hidden);
	}
	
	if ((device == NULL) || (!device->hidden)) {
		SDL_OutOfMemory();
		if (device) {
			if (device->hidden)
				free(device->hidden);
			free(device);
		}
		return(0);
	}

	/* set the function pointers */
	device->VideoInit        = OVLWM_VideoInit;
	device->ListModes        = OVLWM_ListModes;
	device->SetVideoMode     = OVLWM_SetVideoMode;
	device->ToggleFullScreen = NULL;
	device->UpdateMouse      = NULL;
	device->CreateYUVOverlay = NULL;
	device->SetColors        = OVLWM_SetColors;
	device->UpdateRects      = OVLWM_UpdateRects;
	device->VideoQuit        = OVLWM_VideoQuit;
	device->FlipHWSurface    = OVLWM_FlipHWSurface;
	device->LockHWSurface    = OVLWM_LockHWSurface;
	device->UnlockHWSurface  = OVLWM_UnlockHWSurface;
	device->AllocHWSurface   = OVLWM_AllocHWSurface;
	device->FreeHWSurface    = OVLWM_FreeHWSurface;
	device->SetCaption       = OVLWM_SetCaption;
	device->SetIcon          = NULL;
	device->IconifyWindow    = NULL;
	device->GrabInput        = OVLWM_GrabInput;
	device->WarpWMCursor     = OVLWM_WarpWMCursor;
	device->InitOSKeymap     = OVLWM_InitOSKeymap;
	device->PumpEvents       = OVLWM_PumpEvents;
	device->free             = OVLWM_DeleteDevice;

	return device;
}


VideoBootStrap OVLWM_bootstrap = {
	OVLWMVID_DRIVER_NAME, "SDL ovlwm video driver",
	OVLWM_Available, OVLWM_CreateDevice
};


int OVLWM_VideoInit(_THIS, SDL_PixelFormat *vformat) {
	INFO(LOG_Enter("");)

	/* fill the vformat structure with the "best" pixel format */
	vformat->BitsPerPixel = 16;
	vformat->BytesPerPixel = 2;

	/* initialize ovlwm */
	ovl_screen_init(NULL);

	/* We're done! */
	return(0);
}


/*
 * Note:  If we are terminated, this could be called in the middle of
 * another SDL video routine -- notably UpdateRects.
 */
void OVLWM_VideoQuit(_THIS) {
	INFO(LOG_Enter("");)

	OVLWM_RemoveApp(this);
}


SDL_Rect **OVLWM_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags) {
	INFO(LOG_Enter("(bpp=%i, Bpp=%i, flags=%8x)", format->BitsPerPixel, format->BytesPerPixel, flags);)

	/* reject unsupported flags */
	if (flags & (SDL_OPENGL)) return NULL;
	/* reject unsupported pixel formats */
	if (format->BitsPerPixel != 16)
		return NULL;
	if (format->BytesPerPixel) {
		if (format->BytesPerPixel != 2)
			return NULL;
	}

	/* any resolution is available */
	return (SDL_Rect **) -1;
}


void OVLWM_RemoveApp(_THIS) {
	INFO(LOG_Enter("");)
	if (this->hidden->have_app) {
		OVLWM_RemoveEventHandler(this);
		this->hidden->have_app=0;
	}
}
