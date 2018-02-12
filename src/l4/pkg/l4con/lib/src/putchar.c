/*!
 * \file	con/lib/src/putchar.c
 *
 * \brief	putchar functions
 *
 * \author	Mathias Noack <mn3@os.inf.tu-dresden.de>
 *
 */

#include <stdio.h>

/* intern */
#include "internal.h"
#include <l4/log/log_printf.h>

/*****************************************************************************/
/**
 * \brief   Write a character
 *
 * \param   c        ... character
 *
 * \return  a character
 *
 * This function writes a character. (libc)
 */
/*****************************************************************************/
#undef putchar
int
putchar(int c)
{
  if (__init)
    return contxt_putchar(c);

  return LOG_putchar(c);
}

/*****************************************************************************/
/**
 * \brief   Write a character
 *
 * \param   c        ... character
 *
 * \return  a character
 *
 * This function writes a character. (libcontxt)
 */
/*****************************************************************************/
int
contxt_putchar(int c)
{
  if (__init)
    {
      char str[1] = { c };
      contxt_write(str, 1);
    }
  else
     return LOG_putchar(c);
  
  return c;
}

