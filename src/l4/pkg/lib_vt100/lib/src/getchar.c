/**
 * \file   l4vfs/term_server/lib/vt100/getchar.c
 * \brief  read char from the read buffer
 *
 * \date   08/10/2004
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2004-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/lib_vt100/vt100.h>

#include <string.h>

#include "lib.h"

/*****************************************************************************/
/**
 * \brief   Read a character
 *
 * \return  a character
 *
 * This function reads a character.
 */
/*****************************************************************************/

extern int _DEBUG;

int vt100_getchar(termstate_t * term)
{
    int c;

    do
    {
        vt100_semaphore_down(&term->keysem);
        c = term->keylist[term->keylist_next_read];
    } while (c == 0);

//    LOGd(_DEBUG, "c= (%d)", c);

    vt100_semaphore_down(&term->keybufsem);
    term->keylist_next_read =
        (term->keylist_next_read + 1) % KEYLIST_SIZE;
    vt100_semaphore_up(&term->keybufsem);

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
int vt100_trygetchar(termstate_t * term)
{
    int c;

    if (vt100_semaphore_try_down(&term->keysem))
    {
        vt100_semaphore_down(&term->keybufsem);
        c = term->keylist[term->keylist_next_read];
        term->keylist_next_read =
            (term->keylist_next_read + 1) % KEYLIST_SIZE;

        vt100_semaphore_up(&term->keybufsem);
        return c;
    }

    return -1;
}
