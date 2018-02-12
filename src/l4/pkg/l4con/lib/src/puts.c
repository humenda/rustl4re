/*!
 * \file	con/lib/src/puts.c
 *
 * \brief	putchar functions
 *
 * \author	Mathias Noack <mn3@os.inf.tu-dresden.de>,
 *		Jork <jork@os.inf.tu-dresden.de>
 *
 */

/* intern */
#include "internal.h"
#include <l4/log/log_printf.h>
#include <string.h>

/**\brief   Write a string+\n
 *
 * \param   s        ... character
 * \return  >=0 ok, EOF else
 */
int
puts(const char*s)
{
  if (__init)
    return contxt_puts(s);

  return LOG_puts(s);

}

/**\brief   Write a string+\n
 *
 * \param   s        ... character
 * \return  >=0 ok, EOF else
 */
int
contxt_puts(const char *s)
{
  if (__init)
    {
      int ret = contxt_write(s, strlen(s));
      contxt_putchar('\n');
      return ret + 1;
    }
  return LOG_puts(s);
}
