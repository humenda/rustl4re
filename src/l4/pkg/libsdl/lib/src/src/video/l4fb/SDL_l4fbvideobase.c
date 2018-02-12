

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

#include <l4/re/c/video/view.h>
#include <l4/re/c/namespace.h>
#include <l4/re/c/dataspace.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <stdio.h>


static int L4FB_Available(void) {
	return 1;
}

static void L4FB_DeleteDevice(SDL_VideoDevice *device);

static SDL_VideoDevice *L4FB_CreateDevice(int devindex) {
	SDL_VideoDevice *device;
	int err;
        (void)devindex;
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
	device->VideoInit = L4FB_VideoInit;
	device->ListModes = L4FB_ListModes;
	device->SetVideoMode = L4FB_SetVideoMode;
	device->ToggleFullScreen = L4FB_ToggleFullScreen;
	device->UpdateMouse = NULL;
	device->CreateYUVOverlay = NULL;
	device->SetColors = L4FB_SetColors;
	device->UpdateRects = L4FB_UpdateRects;
	device->VideoQuit = L4FB_VideoQuit;
	device->FlipHWSurface = L4FB_FlipHWSurface;
	device->LockHWSurface = L4FB_LockHWSurface;
	device->UnlockHWSurface = L4FB_UnlockHWSurface;
	device->AllocHWSurface = L4FB_AllocHWSurface;
	device->FreeHWSurface = L4FB_FreeHWSurface;
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
	device->SetCaption = L4FB_SetCaption;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = L4FB_GrabInput;
	device->WarpWMCursor = L4FB_WarpWMCursor;
	device->InitOSKeymap = L4FB_InitOSKeymap;
	device->PumpEvents = L4FB_PumpEvents;
	device->free = L4FB_DeleteDevice;
	
	/* get video DS */
	if (l4re_util_video_goos_fb_setup_name(&device->hidden->goosfb, "fb"))
	{
		printf("could not setup fb\n");
		return 0;
	}

	if (l4re_util_video_goos_fb_view_info(&device->hidden->goosfb,
	                                      &device->hidden->vvi))
	{
		printf("could not get fb_info\n");
		return 0;
	}

	device->hidden->fbmem_vaddr
		= l4re_util_video_goos_fb_attach_buffer(&device->hidden->goosfb);

	if (!device->hidden->fbmem_vaddr)
	{
		printf("Failed to attach fb buffer\n");
		return 0;
	}

	device->gl_config.red_size = device->hidden->vvi.pixel_info.r.size;
	device->gl_config.green_size = device->hidden->vvi.pixel_info.g.size;
	device->gl_config.blue_size = device->hidden->vvi.pixel_info.b.size;
	device->gl_config.alpha_size = 0;
	device->gl_config.buffer_size = 0;
	device->gl_config.depth_size = l4re_video_bits_per_pixel(&device->hidden->vvi.pixel_info);


	/* looking for event stuff? look in SDL_l4fbevents.c */
	return device;
}

static void L4FB_DeleteDevice(SDL_VideoDevice *device) {
	free(device->hidden);
	free(device);
}

VideoBootStrap L4FB_bootstrap = {
	L4FBVID_DRIVER_NAME, "SDL fb video driver",
	L4FB_Available, L4FB_CreateDevice
};


int L4FB_VideoInit(_THIS, SDL_PixelFormat *vformat) {
	// fill the vformat structure with the "best" pixel format
	vformat->BitsPerPixel = l4re_video_bits_per_pixel(&this->hidden->vvi.pixel_info);
	vformat->BytesPerPixel = this->hidden->vvi.pixel_info.bytes_per_pixel;
	vformat->Rmask = ~(~0UL << this->hidden->vvi.pixel_info.r.size) << this->hidden->vvi.pixel_info.r.shift;
	vformat->Gmask = ~(~0UL << this->hidden->vvi.pixel_info.g.size) << this->hidden->vvi.pixel_info.g.shift;
	vformat->Bmask = ~(~0UL << this->hidden->vvi.pixel_info.b.size) << this->hidden->vvi.pixel_info.b.shift;
	printf("VideoInit: %d %d %x %x %x\n", vformat->BitsPerPixel, vformat->BytesPerPixel, vformat->Rmask, vformat->Gmask, vformat->Bmask);

	/* We're done! */
	return(0);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void L4FB_VideoQuit(_THIS) {
        (void)this;
	// close windows
	// TODO: DEINIT */
}

SDL_Rect **L4FB_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags) {
        (void)this;

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

