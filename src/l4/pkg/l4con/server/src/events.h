/**
 * \file   con/server/src/events.h
 * \brief  Event handling
 *
 * \date   01/03/2004
 * \author Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 */

/*
 * (c) 2003-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#ifndef CON_EVENTS_H
#define CON_EVENTS_H

/**
 * event notification thread priority
 */
#define _CON_EVENT_THREAD_PRIORITY	(14)

/* init event handling */
void init_events(void);

#endif /* CON_EVENTS_H */
