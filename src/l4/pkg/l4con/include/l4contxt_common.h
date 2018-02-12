/*!
 * \file	l4con/include/l4contxt_common.h
 * \brief	libcontxt common client interface (intern)
 *
 * \date	2002
 * \author	Mathias Noack <mn3@os.inf.tu-dresden.de> */
/*
 * (c) 2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef _L4CONTXT_COMMON_L4CONTXT_COMMON_H
#define _L4CONTXT_COMMON_L4CONTXT_COMMON_H

/* con includes */
#include <l4/l4con/l4con.h>

/** input history buffer */
typedef struct contxt_ihb
{
  char *buffer;		/**< input history buffer head */
  int  first;		/**< first history line */
  int  last;		/**< last history line */
  int  lines;		/**< history buffer size in lines */
  int  length;		/**< max number of characters per line */
} contxt_ihb_t;

/** Initialize input history buffer.
 * \ingroup contxt_if
 *
 * \param   ihb            ... input history buffer structure
 * \param   count          ... number of lines
 * \param   length         ... number of characters per line
 *          
 * This is the init-function of the input history buffer. The history
 * buffer has to be already allocated. See the \b run example of the
 * loader. */
L4_CV int contxt_ihb_init(contxt_ihb_t* ihb, int count, int length);

/** Add string to history buffer
 * \ingroup contxt_if
 *
 * \param   ihb            ... input history buffer structure
 * \param   s              ... string to add
 */
L4_CV void contxt_ihb_add(contxt_ihb_t* ihb, const char *s);

/** Reads a maximum amount of count of characters from keyboard
 * \ingroup contxt_if
 *
 * \param   maxlen         ... size of return string buffer
 *
 * \retval  retstr         ... return string buffer
 * \retval  ihb            ... used input history buffer
 *                             if 0, no input history buffer will be used
 *
 * This function reads a number (maximum maxlen) of character. */
L4_CV void contxt_ihb_read(char* retstr, int maxlen, contxt_ihb_t* ihb);

/** Read a character from keyboarde
 * \ingroup contxt_if
 *
 * \return  a character
 *
 * This function reads a character. (libc) */
#undef getchar /* dietlibc/uClibc define getchar as macro */
L4_CV int getchar(void);

/** Try to get next character. Return -1 if no character is available */
L4_CV int trygetchar(void);

/** Read a character
 * \ingroup contxt_if
 *
 * \return  a character
 *
 * This function reads a character. */
L4_CV int contxt_getchar(void);

/** Try to get next character.
 * \return -1 if no character is available. */
L4_CV int contxt_trygetchar(void);

/** Try to get next character. Return -1 no character is available. */
L4_CV int direct_cons_trygetchar(void);

/** Write a character
 * \ingroup contxt_if
 *
 * \param   c        ... character
 *
 * \return  a character
 *
 * This function writes a character. (libc) */
#undef putchar /* dietlibc/uClibc define putchar as macro */
L4_CV int putchar(int c);

/** Write a character
 * \ingroup contxt_if
 *
 * \param   c        ... character
 *
 * \return  a character
 *
 * This function writes a character. */
L4_CV int contxt_putchar(int c);

/** Write a string + \n
 * \ingroup contxt_if
 *
 * \param   s        ... string
 * \return  >=0 ok, EOF else
 *
 * This function writes a string + \n. (libc) */
L4_CV int puts(const char *s);

/** Write a string + \n
 * \ingroup contxt_if
 *
 * \param   s        ... string
 * \return  >=0 ok, EOF else
 *
 * This function writes a string to the virtual console. */
L4_CV int contxt_puts(const char *s);

/** set graphic mode
 * \ingroup contxt_if
 * 
 * \param   gmode  ... coded graphic mode
 *
 * \return  0 on success (set graphic mode)
 *          
 * Does nothing since the graphics mode is initialized at boot time. */
L4_CV int contxt_set_graphmode(long gmode);

/** get graphic mode
 * \ingroup contxt_if
 * 
 * \return  gmode (graphic mode)
 *          
 * Does nothing since the graphics mode is initialized at boot time. */
L4_CV int contxt_get_graphmode(void);

/** clear screen
 * \ingroup contxt_if
 *          
 * This function fills the screen with the current background color */
L4_CV void contxt_clrscr(void);

#endif
