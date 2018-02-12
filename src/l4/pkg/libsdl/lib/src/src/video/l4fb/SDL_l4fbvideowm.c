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


char* GetEscaped(char *source) {
	char *dest;
	int len;
	int s, d=0;

	len = strlen(source);

	// escaped one can be at most twice as long
	dest = (char*) malloc(len*2+1);

	if (dest) {
		for (s=0; s<=len; s++) {
			switch (source[s]) {
				case '"':
					dest[d++] = '\\';
					break;
			}
			dest[d++] = source[s];
		}
	}

	return dest;
}

void L4FB_SetCaption(_THIS, const char *wintitle, const char *apptitle) {
        (void)this;
	 /* TODO: */
	 /* is there a way to set l4fbs title? */
	 //printf("%s, %s\n", wintitle, apptitle);
}

