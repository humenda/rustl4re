/*
 * \brief   DOpE tokenizer module
 * \date    2002-11-13
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This module splits a given DOpE command string
 * into its tokens. It returns a table of offsets
 * and lengths of the tokens.
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
#include "tokenizer.h"

int init_tokenizer(struct dope_services *d);

/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

/*** RETURNS 1 IF THE SPECIFIED CHARACTER IS A NUMBER-CHARACTER ***/
static s16 is_number_char(char c) {
	if ((c >= '0') && (c <= '9')) return 1;
	if (c == '.') return 1;
	return 0;
}

/*** RETURNS 1 IF THE SPECIFIED CHARACTER IS A IDENTIFIER-CHARACTER ***/
static s16 is_ident_char(char c) {
	if ((c >= 'a') && (c <= 'z')) return 1;
	if ((c >= 'A') && (c <= 'Z')) return 1;
	if ((c >= '0') && (c <= '9')) return 1;
	if ( c == '_') return 1;
	return 0;
}

/*** RETURNS 1 IF THE SPECIFIED CHARACTER IS A IDENTIFIER-CHARACTER ***/
static s16 is_op_char(char c) {
	switch (c) {
		case '(':
		case ')':
		case '.':
		case ',':
		case '=':
		case '"':
		case ' ':
		case '\t':
		case '-':
		  return 1;
		default:
		  return 0;
	}
}

/*** RETURNS TYPE OF TOKEN IN THE GIVEN STRING AT A GIVEN OFFSET ***/
static int token_type(const char *s, u32 offset) {
	if (!s) return TOKEN_WEIRD;

	/* check if first token character is a 'special' character */
	switch (s[offset]) {
		case '(':
		case ')':
		case '.':
		case ',':
		case '=':
			return TOKEN_STRUCT;
		case 0:
			return TOKEN_EOS;
		case '"':
			return TOKEN_STRING;
		case ' ':
		case '\t':
			return TOKEN_EMPTY;
		case '-':
			if (is_number_char(s[offset+1])) return TOKEN_NUMBER;
			if (is_ident_char(s[offset+1])) return TOKEN_IDENT;
	}

	/* check if first character is a number */
	if (is_number_char(s[offset])) return TOKEN_NUMBER;

	/* check if first character is a identifier-character */
	if (is_ident_char(s[offset])) return TOKEN_IDENT;

	return TOKEN_WEIRD;
}


static int ident_size(const char *s) {
	int result=1;
	s++;
	while (is_ident_char(*(s++))) result++;
	return result;
}


static int number_size(const char *s) {
	int result=1;
	s++;
	while (!is_op_char(*(s++))) result++;
	return result;
}


static int string_size(const char *s) {
	int result=1;
	s++;
	while (((*s) != 0) && (*s != '"')) {
		if (*s == '\\') {
			if (*(s+1)=='"') {
				s+=2;
				result+=2;
				continue;
			}
		}
		s++;
		result++;
	}
	if ((*s) == 0) return 1;    /* unclosed string error */
	return result+1;
}


static int token_size(const char *s, u32 offset) {
	switch (token_type(s,offset)) {
		case TOKEN_STRUCT:  return 1;
		case TOKEN_IDENT:   return ident_size(s+offset);
		case TOKEN_NUMBER:  return number_size(s+offset);
		case TOKEN_STRING:  return string_size(s+offset);
		default:            return 1;
	}
}


static int skip_space(const char *s, u32 offset) {
	while ((s[offset] == ' ') || (s[offset] == '\t')) offset++;
	return offset;
}



/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

static int parse(const char *s, u32 max_tok, u32 *offbuf, u32 *lenbuf) {
  (void)max_tok;
	u32 num_tok = 0;
	u32 offset  = 0;

	/* go to first token of the string */
	while ((*(s + offset)) != 0) {
		offset = skip_space(s, offset);
		*offbuf = offset;
		*lenbuf = token_size(s, offset);
		offset += *lenbuf;
		lenbuf++;
		offbuf++;
		num_tok++;
	}

	return num_tok;
}



/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct tokenizer_services services = {
	parse,
	token_type
};



/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_tokenizer(struct dope_services *d) {

	d->register_module("Tokenizer 1.0",&services);
	return 1;
}
