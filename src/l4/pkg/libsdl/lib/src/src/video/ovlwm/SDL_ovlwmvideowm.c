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
//#include <l4/dope/dopelib.h>
//#include <l4/dope/vscreen.h>
#include <l4/overlay_wm/ovl_screen.h>
//
//char* GetEscaped(char *source) {
//	char *dest;
//	int len;
//	int s, d=0;
//
//	len = strlen(source);
//
//	// escaped one can be at most twice as long
//	dest = (char*) malloc(len*2+1);
//
//	if (dest) {
//		for (s=0; s<=len; s++) {
//			switch (source[s]) {
//				case '"':
//					dest[d++] = '\\';
//					break;
//			}
//			dest[d++] = source[s];
//		}
//	}
//
//	return dest;
//}

void OVLWM_SetCaption(_THIS, const char *wintitle, const char *apptitle) {
//	char* title;
//
//	INFO(LOG_Enter("(wintitle=\"%s\", apptitle=\"%s\")", wintitle, apptitle);)
//
//	if (this->hidden->have_app) {
//		title = GetEscaped((char*) wintitle);
//		if (title) {
//			dope_cmdf(this->hidden->app_id, "win.set(-title \"%s\")", title);
//			free(title);
//		}
//	}
}

