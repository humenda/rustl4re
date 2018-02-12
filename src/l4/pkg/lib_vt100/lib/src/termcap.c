/**
 * \file   l4vfs/term_server/lib/vt100/termcap.c
 * \brief  some termcap functionality
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

#include <termios.h>
#include <unistd.h>
#include <string.h>

#include "lib.h"

extern int _DEBUG;

int vt100_tcgetattr( termstate_t *term, struct termios *termios_p )
{
    termios_p->c_iflag = 0;
    // currently we do not need any output flags
    termios_p->c_oflag = 0;
    // the same for control flags
    termios_p->c_cflag = 0;

    // build local flags
    termios_p->c_lflag = 0;
    if (term->echo)
        termios_p->c_lflag |= ECHO;
    if (term->term_mode==VT100MODE_COOKED)
        termios_p->c_lflag |= ICANON;


    // list of the known control characters
    termios_p->c_cc[VEOF]   = CEOF;
    termios_p->c_cc[VEOL]   = _POSIX_VDISABLE;
    termios_p->c_cc[VEOL2]  = _POSIX_VDISABLE;
    termios_p->c_cc[VERASE] = CERASE;
    termios_p->c_cc[VWERASE]= CWERASE;
    termios_p->c_cc[VKILL]  = CKILL;
    termios_p->c_cc[VREPRINT]=CREPRINT;
    termios_p->c_cc[VINTR]  = CINTR;
    termios_p->c_cc[VQUIT]  = _POSIX_VDISABLE;
    termios_p->c_cc[VSUSP]  = CSUSP;
    termios_p->c_cc[VSTART] = CSTART;
    termios_p->c_cc[VSTOP]  = CSTOP;
    termios_p->c_cc[VLNEXT] = CLNEXT;
    termios_p->c_cc[VDISCARD]=CDISCARD;
    termios_p->c_cc[VMIN]   = CMIN;
    termios_p->c_cc[VTIME]  = 0;

    return 0;
}

int vt100_tcsetattr( termstate_t *term, struct termios *termios_p )
{
    // actually we do only care for the ECHO and ICANON flags
    if (termios_p->c_lflag & ICANON)
    {
        term->term_mode = VT100MODE_COOKED;
    }
    else
    {
        term->term_mode = VT100MODE_RAW;
    }

    if (termios_p->c_lflag & ECHO)
    {
        term->echo = 1;
    }
    else
    {
        term->echo = 0;
    }

    return 0;
}

int vt100_getwinsize( termstate_t *term, struct winsize *win )
{
    win->ws_row     = term->phys_h;
    win->ws_col     = term->w;
    win->ws_xpixel  = 0;
    win->ws_ypixel  = 0;

    return 0;
}
