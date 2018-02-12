/*!
 * \file	con/lib/src/getchar.c
 *
 * \brief	getchar functions
 *
 * \author	Mathias Noack <mn3@os.inf.tu-dresden.de>
 *
 */

#include <stdio.h>

/* intern */
#include "internal.h"
#include "keymap.h"
#include "evh.h"
#include <stdio.h>


/*****************************************************************************/
/**
 * \brief   Read a character
 *
 * \return  a character
 *
 * This function reads a character. (libcontxt) 
 */
/*****************************************************************************/
int
contxt_getchar(void)
{
  int c;

  if (!__init)
    return 0;

  _cursor(SHOW);
  do
    {
      l4semaphore_down(&keysem);
      c = keymap[keylist[keylist_tail]][__shift];
      keylist_tail = (keylist_tail + 1) % CONTXT_KEYLIST_SIZE;
      __keyin = 1;
    } while (c == 0);
  _cursor(HIDE);
  
  return c;
}

/*****************************************************************************/
/**
 * \brief   Try to read a character
 *
 * \return  a character
 *
 * This function tries to read a character.
 */
/*****************************************************************************/
int
contxt_trygetchar(void)
{
  int c;
  
  if (!__init)
    return 0;
  
  if (l4semaphore_try_down(&keysem) == 1)
    {
      c = keymap[keylist[keylist_tail]][__shift];
      keylist_tail = (keylist_tail + 1) % CONTXT_KEYLIST_SIZE;
      __keyin = 1;
      return c;
    }

  return -1;
}

/*****************************************************************************/
/**
 * \brief   Read a character
 *
 * \return  a character
 *
 * This function reads a character. (libc) 
 */
/*****************************************************************************/
#undef getchar
int 
getchar(void)
{
  return contxt_getchar();
}


int
trygetchar(void)
{
  return contxt_trygetchar();
}

int
direct_cons_trygetchar(void)
{
  return contxt_trygetchar();
}

