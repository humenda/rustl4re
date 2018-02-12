#ifndef _SDL_nullaudio_h
#define _SDL_nullaudio_h

#include "SDL_sysaudio.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_AudioDevice *this

struct SDL_PrivateAudioData {
	Uint8 *mixbuf;
	float frame_ticks;
	float next_frame;
};

#define FUDGE_TICKS     3
#define frame_ticks     (this->hidden->frame_ticks)
#define next_frame      (this->hidden->next_frame)

#endif /* _SDL_nullaudio_h */
