#ifndef _SDL_dropsaudio_h
#define _SDL_dropsaudio_h

#include "SDL_sysaudio.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_AudioDevice *this

struct SDL_PrivateAudioData {
	Uint8 *mixbuf;
    Uint32 mixlen;
};

#endif /* _SDL_dropsaudio_h */
