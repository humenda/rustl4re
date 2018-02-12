/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/* Based on SDL_config_minimal.h */

#ifndef _SDL_config_l4re_h
#define _SDL_config_l4re_h

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "SDL_platform.h"

/* This is the minimal configuration that can be used to build SDL */

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>


#define HAVE_LIBC 1

#define HAVE_ALLOCA_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_STDIO_H 1
#define STDC_HEADERS 1
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
#define HAVE_MALLOC_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_CTYPE_H 1
#define HAVE_MATH_H 1
/* #undef HAVE_ICONV_H */
#undef HAVE_SIGNAL_H

#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC 1
#define HAVE_FREE 1
#define HAVE_ALLOCA 1
#define HAVE_QSORT 1
#define HAVE_ABS 1
#define HAVE_BCOPY 1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMCMP 1
#define HAVE_STRLEN 1
/* #undef HAVE_STRLCPY */
/* #undef HAVE_STRLCAT */
#define HAVE_STRDUP 1
/* #undef HAVE__STRREV */
/* #undef HAVE__STRUPR */
/* #undef HAVE__STRLWR */
/* #undef HAVE_INDEX */
/* #undef HAVE_RINDEX */
#define HAVE_STRCHR 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
/* #undef HAVE_ITOA */
/* #undef HAVE__LTOA */
/* #undef HAVE__UITOA */
/* #undef HAVE__ULTOA */
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
/* #undef HAVE__I64TOA */
/* #undef HAVE__UI64TOA */
#define HAVE_STRTOLL 1
#define HAVE_STRTOULL 1
#define HAVE_STRTOD 1
#define HAVE_ATOI 1
#define HAVE_ATOF 1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP 1
/* #undef HAVE__STRICMP */
#define HAVE_STRCASECMP 1
/* #undef HAVE__STRNICMP */
#define HAVE_STRNCASECMP 1
#define HAVE_SSCANF 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_ICONV 1
/* #undef HAVE_SIGACTION */
#define HAVE_SETJMP 1
#define HAVE_NANOSLEEP 1
/* #undef HAVE_CLOCK_GETTIME */
#define HAVE_GETPAGESIZE 1
/* undef HAVE_MPROTECT */

#define SDL_THREAD_PTHREAD 1
#define SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1


#define DISABLE_JOYSTICK 1
#define DISABLE_CDROM 1
#define ENABLE_L4FBVIDEO 1
#define ENABLE_NULLAUDIO 1
#define FPROV 1
#define NO_SIGNAL_H 1
#define ENABLE_PTHREADS 1
#define SDL_USE_PTHREADS 1
#define SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1
#define L4RE 1

/* Enable the dummy audio driver (src/audio/dummy/\*.c) */
#define SDL_AUDIO_DRIVER_DUMMY	1

/* Enable the stub cdrom driver (src/cdrom/dummy/\*.c) */
#define SDL_CDROM_DISABLED	1

/* Enable the stub joystick driver (src/joystick/dummy/\*.c) */
#define SDL_JOYSTICK_DISABLED	1

/* Enable the dummy video driver (src/video/dummy/\*.c) */
#define SDL_VIDEO_DRIVER_DUMMY	1

#endif /* _SDL_config_l4re_h */
