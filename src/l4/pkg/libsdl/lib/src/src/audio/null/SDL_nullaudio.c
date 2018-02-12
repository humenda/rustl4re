#include <malloc.h>
#include <string.h>

#include "SDL_audio.h"
#include "SDL_error.h"
#include "SDL_audiomem.h"
#include "SDL_audio_c.h"
#include "SDL_timer.h"
#include "SDL_audiodev_c.h"
#include "SDL_nullaudio.h"

int NULLAUD_available __attribute__ ((weak)) = 0;

/* Audio driver functions */
static int    NULLAUD_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void   NULLAUD_WaitAudio(_THIS);
static void   NULLAUD_PlayAudio(_THIS);
static Uint8 *NULLAUD_GetAudioBuf(_THIS);
static void   NULLAUD_CloseAudio(_THIS);

/* Audio driver bootstrap functions */
static int NULLAUD_Available(void) {
	return NULLAUD_available;
}

static void NULLAUD_DeleteDevice(SDL_AudioDevice *device) {
	free(device->hidden);
	free(device);
}

static SDL_AudioDevice *NULLAUD_CreateDevice(int devindex) {
	SDL_AudioDevice *this;

	// Initialize device struct
	this = (SDL_AudioDevice *) malloc(sizeof(SDL_AudioDevice));
	if ( this ) {
		memset(this, 0, (sizeof *this));
		this->hidden = (struct SDL_PrivateAudioData *)
		malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( this ) {
			free(this);
		}
		return(0);
	}
	memset(this->hidden, 0, (sizeof *this->hidden));

	// Set the function pointers
	this->OpenAudio   = NULLAUD_OpenAudio;
	this->WaitAudio   = NULLAUD_WaitAudio;
	this->PlayAudio   = NULLAUD_PlayAudio;
	this->GetAudioBuf = NULLAUD_GetAudioBuf;
	this->CloseAudio  = NULLAUD_CloseAudio;
	this->free        = NULLAUD_DeleteDevice;

	return this;
}

AudioBootStrap NULLAUD_bootstrap = {
	"NULL",
	"dummy audio driver w/o output",
	NULLAUD_Available,
	NULLAUD_CreateDevice
};

static void NULLAUD_WaitAudio(_THIS) {
	Sint32 ticks;

	ticks = ((Sint32)(next_frame - SDL_GetTicks()))-FUDGE_TICKS;
	if ( ticks > 0 ) {
		SDL_Delay(ticks);
	}
}

static void NULLAUD_PlayAudio(_THIS) {
	if (! next_frame)
		next_frame = SDL_GetTicks();

	next_frame += frame_ticks;
}

static Uint8 *NULLAUD_GetAudioBuf(_THIS) {
	return this->hidden->mixbuf;
}

static void NULLAUD_CloseAudio(_THIS) {
}

static int NULLAUD_OpenAudio(_THIS, SDL_AudioSpec *spec) {
	this->hidden->mixbuf=malloc(spec->size);
	if (! this->hidden->mixbuf) {
		SDL_OutOfMemory();
		return -1;
	}

	printf("Warning: using null audio driver\n");
	printf("%i Hz %i channels, format 0x%04x\n", spec->freq, spec->channels, spec->format);
	SDL_Delay(2000);

	frame_ticks = (float)(spec->samples*1000)/spec->freq;
	next_frame = 0;

	return 0;
}

