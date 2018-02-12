/*
 * \brief   DOpE utility functions
 * \date    2003-08-02
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This file contains several utility functions used by
 * other components. This way inconsistencies between
 * different libC functionalities can be avoided by just
 * not using libC.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include "dopestd.h"


/*** CONVERT A FLOAT INTO A STRING ***/
int dope_ftoa(float v, int prec, char *dst, int max_len) {
	int dig = 0, neg = 0, zero = 0;

	if (!dst) return -1;

	if (v < 0) { v = -v; dig++; neg = 1; }
	
	for (; (int)v > 0; v = v/10.0) dig++;
	
	if (dig == 0) { dig = 1; zero = 1; }
	
	if (dig+prec+2 > max_len) {
		prec = max_len - dig - 1;
		if (prec < 0) {
			*dst = 0;
			return -1;
		}
	}
	
	if (neg)  { *(dst++) = '-'; dig--; }
	if (zero) { *(dst++) = '0'; dig--; }
	
	for (;dig-->0;) {
		v = v*10;
		*(dst++) = '0' + (int)v;
		v = v - (int)v;
	}
	
	if (prec) *(dst++) = '.';
	
	for (;prec--;) {
		v = v*10;
		*(dst++) = '0' + (int)v;
		v = v - (int)v;
	}
	*dst = 0;
	return 0;
}


/*** DETERMINES IF TWO STRINGS ARE EQUAL ***/
int dope_streq(const char *s1, const char *s2, int max_s1) {
	int i;
	if (!s1 || !s2) return 0;
	for (i=0; i<max_s1; i++) {
		if (*s1 != *s2) return 0;
		if ((*s1 == 0) && (*s2 == 0)) return 1;
		s1++; s2++;
	}

	/* if s2 has the same length as s1, we are fine */
	if ((i == max_s1) && (*s2 == 0)) return 1;
	return 0;
}


/*** DUPLICATE STRING ***/
char *dope_strdup(char *s) {
	char *d;
	char *result;
	s32 strl;
	if (!s) return NULL;
	strl = strlen(s);
	if (strl >= 0) {
		result = malloc(strl+2);
		if (!result) return NULL;
		d = result;
		while (*s) *(d++) = *(s++);
		*d = 0;
		return result;
	}
	return NULL;
}


/*** ALLOCATE MEMORY BLOCK AND SET TO ZERO ***/
void *zalloc(unsigned long size) {
	void *ret = malloc(size);
	if (ret) memset(ret, 0, size);
	return ret;
}
