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
#include <l4/names/libnames.h>
#include <l4/thread/thread.h>

static int DOPE_Available(void) {
	l4_threadid_t dope_server;
	INFO(LOG_Enter("");)

	return (names_waitfor_name("DOpE", &dope_server, 1000) != 0);
}

static void DOPE_DeleteDevice(SDL_VideoDevice *device);

static SDL_VideoDevice *DOPE_CreateDevice(int devindex) {
	SDL_VideoDevice *device;

	INFO(LOG_Enter("(devindex=%i)", devindex);)
	SDL_Delay(2000);

	/* Initialize device */
	device = (SDL_VideoDevice*) malloc(sizeof *device);
	if (device) {
		memset(device, 0, sizeof *device);
		device->hidden = (struct SDL_PrivateVideoData*) malloc(sizeof *device->hidden);
		if (device->hidden) {
			memset(device->hidden, 0, sizeof *device->hidden);
		}
	}
	if (
		   (device == NULL)
		|| (device->hidden == NULL)
	) {
		SDL_OutOfMemory();
		if (device) {
			if (device->hidden) {
				free(device->hidden);
			}

			free(device);
		}
		return(0);
	}

	/* Set the function pointers */
	device->VideoInit = DOPE_VideoInit;
	device->ListModes = DOPE_ListModes;
	device->SetVideoMode = DOPE_SetVideoMode;
	device->ToggleFullScreen = DOPE_ToggleFullScreen;
	device->UpdateMouse = NULL;
	device->CreateYUVOverlay = NULL;
	device->SetColors = DOPE_SetColors;
	device->UpdateRects = DOPE_UpdateRects;
	device->VideoQuit = DOPE_VideoQuit;
	device->FlipHWSurface = DOPE_FlipHWSurface;
	device->LockHWSurface = DOPE_LockHWSurface;
	device->UnlockHWSurface = DOPE_UnlockHWSurface;
	device->AllocHWSurface = DOPE_AllocHWSurface;
	device->FreeHWSurface = DOPE_FreeHWSurface;
/*
    // hw accel funs
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
    // gamma
	device->SetGamma = NULL;
	device->GetGamma = NULL;
	device->SetGammaRamp = NULL;
	device->GetGammaRamp = NULL;
*/
	device->SetCaption = DOPE_SetCaption;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = DOPE_GrabInput;
	device->WarpWMCursor = DOPE_WarpWMCursor;
	device->InitOSKeymap = DOPE_InitOSKeymap;
	device->PumpEvents = DOPE_PumpEvents;
	device->free = DOPE_DeleteDevice;

	return device;
}

static void DOPE_DeleteDevice(SDL_VideoDevice *device) {
	INFO(LOG_Enter("");)
	free(device->hidden);
	free(device);
}

VideoBootStrap DOPE_bootstrap = {
	DOPEVID_DRIVER_NAME, "SDL dope video driver",
	DOPE_Available, DOPE_CreateDevice
};


int DOPE_VideoInit(_THIS, SDL_PixelFormat *vformat) {
	INFO(LOG_Enter("");)
	// fill the vformat structure with the "best" pixel format
	vformat->BitsPerPixel = 16;
	vformat->BytesPerPixel = 2;

	// initialize dope
	dope_init();

	/* We're done! */
	return(0);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void DOPE_VideoQuit(_THIS) {
	INFO(LOG_Enter("");)

	// close windows
	DOPE_RemoveApp(this);
	dope_deinit();
}

SDL_Rect **DOPE_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags) {
	INFO(LOG_Enter("(bpp=%i, Bpp=%i, flags=%8x)", format->BitsPerPixel, format->BytesPerPixel, flags);)

	// reject unsupported flags
	if (flags & (SDL_OPENGL)) return NULL;
	// reject unsupported pixel formats
	if (format->BitsPerPixel != 16)
		return NULL;
	if (format->BytesPerPixel) {
		if (format->BytesPerPixel != 2)
			return NULL;
	}

	// any resolution is available
	return (SDL_Rect **) -1;
}

void DOPE_RemoveApp(_THIS) {
	INFO(LOG_Enter("");)
	if (this->hidden->have_app) {
		DOPE_RemoveEventHandler(this);
		this->hidden->have_app=0;
		dope_cmdf(this->hidden->app_id, "win.close()");
		dope_deinit_app(this->hidden->app_id);
	}
}
