#include <malloc.h>
#include <string.h>

#include <l4/log/l4log.h>
#include <l4/dsound/dslib.h>
#include <l4/env/errno.h>

#include "SDL_audio.h"
#include "SDL_error.h"
#include "SDL_audiomem.h"
#include "SDL_audio_c.h"
#include "SDL_timer.h"
#include "SDL_audiodev_c.h"
#include "SDL_dropsaudio.h"

/* Audio driver functions */
static int    DROPSAUD_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void   DROPSAUD_WaitAudio(_THIS);
static void   DROPSAUD_PlayAudio(_THIS);
static Uint8 *DROPSAUD_GetAudioBuf(_THIS);
static void   DROPSAUD_CloseAudio(_THIS);

/* Audio driver bootstrap functions */
static int DROPSAUD_Available(void) {
	int err;

	err = dslib_init(5000); // try to init lib; wait 5 secs max for dsd

	return err?0:1;
}

static void DROPSAUD_DeleteDevice(SDL_AudioDevice *device) {
	free(device->hidden);
	free(device);
}

static SDL_AudioDevice *DROPSAUD_CreateDevice(int devindex) {
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
	this->OpenAudio   = DROPSAUD_OpenAudio;
	this->WaitAudio   = DROPSAUD_WaitAudio;
	this->PlayAudio   = DROPSAUD_PlayAudio;
	this->GetAudioBuf = DROPSAUD_GetAudioBuf;
	this->CloseAudio  = DROPSAUD_CloseAudio;
	this->free        = DROPSAUD_DeleteDevice;

	return this;
}

AudioBootStrap DROPSAUD_bootstrap = {
	"DROPS",
	"DROPS audio using DSlib",
	DROPSAUD_Available,
	DROPSAUD_CreateDevice
};

static void DROPSAUD_WaitAudio(_THIS) {
	// writing full blocks and having set 2 audio fragments
	// so no wait is needed, latency is min
}

static void DROPSAUD_PlayAudio(_THIS) {
	int err;
	long count = this->hidden->mixlen;

	if ((err=dslib_play(&count))<0) {
		LOGl("Error (%i): %s", -err, l4env_errstr(-err));
		LOGl("disabling SDL audio");
		this->enabled = 0;
	}
}

static Uint8 *DROPSAUD_GetAudioBuf(_THIS) {
	return this->hidden->mixbuf;
}

static void DROPSAUD_CloseAudio(_THIS) {
	dslib_close_dsp();
}

static int DROPSAUD_OpenAudio(_THIS, SDL_AudioSpec *spec) {
// We may change spec to supported spec, SDL will convert
	int err;
	long fmts;
	Uint16 test_format;
	long fmt;
	int frag_spec;

	if ((err=dslib_open_dsp(10, (void**) &this->hidden->mixbuf))) {
		SDL_SetError("error opening dsp (%i): %s", -err, l4env_errstr(-err));
		return -1;
	}

	// Get a list of supported hardware formats
	if ((err=dslib_get_fmts(&fmts))) {
		SDL_SetError("Couldn't get audio format list (%i): %s", -err, l4env_errstr(-err));
		return(-1);
	}

	dslib_close_dsp();

	// Try for a closest match on audio format
	while ((test_format = SDL_FirstAudioFormat(spec->format))) {
		switch (test_format) {
			case AUDIO_U8:
				fmt = DSD_AFMT_U8;
				break;
			case AUDIO_S8:
				fmt = DSD_AFMT_S8;
				break;
			case AUDIO_U16LSB:
				fmt = DSD_AFMT_U16_LE;
				break;
			case AUDIO_S16LSB:
				fmt = DSD_AFMT_S16_LE;
				break;
			case AUDIO_U16MSB:
				fmt = DSD_AFMT_U16_BE;
				break;
			case AUDIO_S16MSB:
				fmt = DSD_AFMT_S16_BE;
				break;
			default:
				fmt = 0;
		}
		if (fmts & fmt) break;
	
		test_format = SDL_NextAudioFormat();
	}
	if (! test_format) {
		SDL_SetError("Couldn't find any suitable hardware audio format");
		return(-1);
	}

	spec->format = test_format;

	// recalc audio spec with new format
	SDL_CalculateAudioSpec(spec);

	// Determine the power of two of the fragment size
	for ( frag_spec = 0; (0x01<<frag_spec) < spec->size; ++frag_spec );
	if ( (0x01<<frag_spec) != spec->size ) {
		SDL_SetError("Fragment size must be a power of two");
		return(-1);
	}

	switch (spec->format) {
		case AUDIO_U8:
			fmt = DSD_AFMT_U8;
			break;
		case AUDIO_S8:
			fmt = DSD_AFMT_S8;
			break;
		case AUDIO_U16LSB:
			fmt = DSD_AFMT_U16_LE;
			break;
		case AUDIO_S16LSB:
			fmt = DSD_AFMT_S16_LE;
			break;
		case AUDIO_U16MSB:
			fmt = DSD_AFMT_U16_BE;
			break;
		case AUDIO_S16MSB:
			fmt = DSD_AFMT_S16_BE;
			break;
		default:
			SDL_SetError("Couldn't find any suitable hardware audio format");
			return(-1);
	}

	if ((err=dslib_open_dsp(spec->size, (void**) &this->hidden->mixbuf))) {
		SDL_SetError("error opening dsp (%i): %s", -err, l4env_errstr(-err));
		return -1;
	}
	this->hidden->mixlen = spec->size;
	memset(this->hidden->mixbuf, spec->silence, spec->size);

	// two fragments, for low latency
	if ((err=dslib_set_frag(2, spec->size))) {
		SDL_SetError("error setting fragment size (%i): %s", -err, l4env_errstr(-err));
		dslib_close_dsp();
		return -1;
	}

	// now set the parameters
	if ((err=dslib_set_fmt(fmt))) {
		SDL_SetError("error setting format (%i): %s", -err, l4env_errstr(-err));
		dslib_close_dsp();
		return -1;
	}
	if ((err=dslib_set_chans(spec->channels))) {
		SDL_SetError("error setting #channels (%i): %s", -err, l4env_errstr(-err));
		dslib_close_dsp();
		return -1;
	}
	if ((err=dslib_set_freq(spec->freq))) {
		SDL_SetError("error setting frequency (%i): %s", -err, l4env_errstr(-err));
		dslib_close_dsp();
		return -1;
	}
	if ((err=dslib_set_vol(0xF0F0))) {
		SDL_SetError("error setting volume (%i): %s", -err, l4env_errstr(-err));
		dslib_close_dsp();
		return -1;
	}

	return(0);
}

