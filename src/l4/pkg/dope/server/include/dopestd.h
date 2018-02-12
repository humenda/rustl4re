/*
 * \brief   Standard types and functions used by DOpE
 * \date    2003-08-01
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef _DOPE_DOPESTD_H_
#define _DOPE_DOPESTD_H_

#define SHOW_INFOS  0
#define SHOW_ERRORS 1

#include <string.h>


/**************************
 *** TYPES USED BY DOpE ***
 **************************/

#if defined(L4API_l4f)

#include <inttypes.h>

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t u16;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef intptr_t adr;

#else

typedef unsigned char   u8;
typedef   signed char   s8;
typedef unsigned short u16;
typedef   signed short s16;
typedef unsigned long  u32;
typedef   signed long  s32;
typedef unsigned long  adr;

#endif

#include <stdlib.h>
#include <stdio.h>

void  *zalloc(unsigned long size);

/********************************
 *** IMPLEMENTED IN DOPESTD.C ***
 ********************************/

/*** CONVERT A FLOAT INTO A STRING ***
 *
 * This function performs zero-termination of the string.
 *
 * \param v       float value to convert
 * \param prec    number of digits after comma
 * \param dst     destination buffer
 * \param max_len destination buffer size
 */
extern int dope_ftoa(float v, int prec, char *dst, int max_len);


/*** CHECK IF TWO STRINGS ARE EQUAL ***
 *
 * This function compares two strings s1 and s2. The length of string s1 can
 * be defined via max_s1.  If max_s1 characters are identical and strlen(s2)
 * equals max_s1, the two strings are considered to be equal. This way, s2
 * can be compared against a substring without null-termination.
 *
 * \param s1      string (length is bounded by max_s1)
 * \param s2      null-terminated second string
 * \param max_s1  max length of string s1
 * \return        1 if the two strings are equal.
 */
extern int dope_streq(const char *s1, const char *s2, int max_len);


/*** DUPLICATE STRING ***/
extern char *dope_strdup(char *s);


/*********************************
 *** DEBUG MACROS USED IN DOPE ***
 *********************************/

/*
 * Within the code of DOpE the following macros for filtering
 * debug output are used.
 *
 * INFO    - for presenting status information
 * WARNING - for informing the user about non-serious problems
 * ERROR   - for reporting really worse stuff
 */

#if SHOW_WARNINGS
	#define WARNING(x) x
#else
	#define WARNING(x) /* x */
#endif

#undef INFO
#if SHOW_INFOS
	#define INFO(x) x
#else
	#define INFO(x) /* x */
#endif

#undef ERROR
#if SHOW_ERRORS
	#define ERROR(x) x
#else
	#define ERROR(x) /* x */
#endif


#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/******************************
 *** DOPE SERVICE STRUCTURE ***
 ******************************/

/*
 * DOpE provides the following service structure to all
 * its components. Via this structure components can
 * access functionality of other components or make an
 * interface available to other components.
 */

struct dope_services {
	void *(*get_module)      (const char *name);
	long  (*register_module) (const char *name,void *services);
};

#endif /* _DOPE_DOPESTD_H_ */

